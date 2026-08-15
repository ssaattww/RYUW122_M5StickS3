#include "NtpTimeSynchronizer.h"

#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "EspNowTransport.h"
#include "NodeStatus.h"
#include "TagMasterCoordinator.h"

#include <esp_timer.h>

#include <cstring>
#include <limits>

namespace
{
    constexpr uint64_t TimestampEpochUs = uint64_t{1} << 32;

    /**
     * @brief 2つのMACアドレスが一致するか確認します。
     *
     * @param left 比較する左辺MACアドレス
     * @param right 比較する右辺MACアドレス
     * @return 一致する場合はtrue、それ以外はfalse
     */
    bool IsSameMac(const uint8_t left[6], const uint8_t right[6])
    {
        return memcmp(left, right, 6) == 0;
    }

    /**
     * @brief MACアドレスが有効なユニキャストアドレスか確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return ゼロまたはbroadcastでない場合はtrue、それ以外はfalse
     */
    bool IsValidMac(const uint8_t macAddress[6])
    {
        bool allZero = true;
        bool allBroadcast = true;
        for (size_t index = 0; index < 6; ++index)
        {
            allZero = allZero && macAddress[index] == 0U;
            allBroadcast = allBroadcast && macAddress[index] == 0xffU;
        }
        return !allZero && !allBroadcast;
    }

    /**
     * @brief 2つの64bit時刻の絶対差をオーバーフローせずに返します。
     *
     * @param left 比較する左辺時刻
     * @param right 比較する右辺時刻
     * @return 2時刻の絶対差
     */
    uint64_t AbsoluteDifference(uint64_t left, uint64_t right)
    {
        return left >= right ? left - right : right - left;
    }

}

NtpTimeSynchronizer::NtpTimeSynchronizer(
    EspNowTransport& transport,
    EspNowBroadcast& broadcast,
    TagMasterCoordinator& coordinator,
    ConfigRuntime& configRuntime,
    NtpTimeProvider timeProvider)
    : m_transport(transport),
      m_broadcast(broadcast),
      m_coordinator(coordinator),
      m_configRuntime(configRuntime),
      m_timeProvider(timeProvider == nullptr ? DefaultTimeProvider : timeProvider)
{
}

void NtpTimeSynchronizer::Update()
{
    DetectMasterChange();
    ProcessReceivedPackets();
    TrySendPendingResponse();
    UpdateMaster();
}

bool NtpTimeSynchronizer::IsSynchronizationComplete() const
{
    if (!m_master.isValid)
    {
        return false;
    }
    if (!m_master.isSelfMaster)
    {
        return m_localSynchronization.isValid;
    }
    return !m_requestPending &&
        m_currentTargetIndex >= m_targetCount;
}

/**
 * @brief 現在のローカル単調時刻に対応するマスターTAG基準時刻を取得します。
 *
 * @param masterTimeUs 現在のマスターTAG基準時刻格納先
 * @return 自ノードがマスター、または同期済み非マスターの場合はtrue。それ以外はfalse
 */
bool NtpTimeSynchronizer::TryGetCurrentMasterTime(
    uint64_t& masterTimeUs) const
{
    if (!m_master.isValid)
    {
        return false;
    }

    const uint64_t localTimeUs = m_timeProvider();
    uint64_t currentMasterTimeUs = 0;
    if (m_master.isSelfMaster)
    {
        currentMasterTimeUs = localTimeUs;
    }
    else if (!m_localSynchronization.isValid ||
        !TranslateClockDomain(
            localTimeUs,
            m_localSynchronizationAnchorUs,
            m_localSynchronization.synchronizedAtMasterTimeUs,
            currentMasterTimeUs))
    {
        return false;
    }

    masterTimeUs = currentMasterTimeUs;
    return true;
}

bool NtpTimeSynchronizer::TryGetNodeSynchronization(
    uint8_t nodeId,
    NodeTimeSynchronization& synchronization) const
{
    const NodeTimeSynchronization* found = nullptr;
    if (!m_master.isSelfMaster &&
        m_localSynchronization.isValid &&
        m_localSynchronization.nodeId == nodeId)
    {
        found = &m_localSynchronization;
    }
    else
    {
        for (size_t index = 0; index < m_targetCount; ++index)
        {
            if (m_targets[index].synchronization.isValid &&
                m_targets[index].nodeId == nodeId)
            {
                found = &m_targets[index].synchronization;
                break;
            }
        }
    }
    if (found == nullptr)
    {
        return false;
    }

    synchronization = *found;
    const uint64_t nowUs = m_timeProvider();
    if (found == &m_localSynchronization)
    {
        synchronization.synchronizationAgeUs =
            nowUs >= m_localSynchronizationAnchorUs
                ? nowUs - m_localSynchronizationAnchorUs
                : 0;
    }
    else
    {
        synchronization.synchronizationAgeUs =
            nowUs >= synchronization.synchronizedAtMasterTimeUs
                ? nowUs - synchronization.synchronizedAtMasterTimeUs
                : 0;
    }
    return true;
}

bool NtpTimeSynchronizer::TryConvertNodeTimeToMaster(
    uint8_t nodeId,
    uint32_t nodeLocalTimeUs,
    uint64_t referenceMasterTimeUs,
    uint64_t& masterTimeUs) const
{
    NodeTimeSynchronization synchronization{};
    if (!TryGetNodeSynchronization(nodeId, synchronization))
    {
        return false;
    }

    const uint32_t masterTimestampUs = nodeLocalTimeUs -
        static_cast<uint32_t>(synchronization.nodeMinusMasterUs);
    masterTimeUs = ExtendTimestampNear(
        masterTimestampUs,
        referenceMasterTimeUs);
    return true;
}

bool NtpTimeSynchronizer::TryConvertLocalTimeToMaster(
    uint32_t localTimeUs,
    uint64_t& masterTimeUs) const
{
    if (!m_localSynchronization.isValid)
    {
        return false;
    }
    const uint64_t localReferenceTimeUs = m_timeProvider();
    uint64_t masterReferenceTimeUs = 0;
    if (!TranslateClockDomain(
            localReferenceTimeUs,
            m_localSynchronizationAnchorUs,
            m_localSynchronization.synchronizedAtMasterTimeUs,
            masterReferenceTimeUs))
    {
        return false;
    }
    const uint32_t masterTimestampUs = localTimeUs -
        static_cast<uint32_t>(m_localSynchronization.nodeMinusMasterUs);
    masterTimeUs = ExtendTimestampNear(
        masterTimestampUs,
        masterReferenceTimeUs);
    return true;
}

int64_t NtpTimeSynchronizer::ModuloDifference(
    uint32_t later,
    uint32_t earlier)
{
    return static_cast<int64_t>(static_cast<int32_t>(later - earlier));
}

bool NtpTimeSynchronizer::CalculateSample(
    uint32_t t1,
    uint32_t t2,
    uint32_t t3,
    uint32_t t4,
    NtpTimeSample& sample)
{
    sample = NtpTimeSample{};
    const int64_t t2MinusT1 = ModuloDifference(t2, t1);
    const int64_t t3MinusT4 = ModuloDifference(t3, t4);
    const int64_t t4MinusT1 = ModuloDifference(t4, t1);
    const int64_t t3MinusT2 = ModuloDifference(t3, t2);
    const int64_t roundTripUs = t4MinusT1 - t3MinusT2;
    if (roundTripUs < 0 ||
        roundTripUs > static_cast<int64_t>(m_responseTimeoutUs))
    {
        return false;
    }

    sample.nodeMinusMasterUs = (t2MinusT1 + t3MinusT4) / 2;
    sample.roundTripUs = static_cast<uint32_t>(roundTripUs);
    sample.isValid = true;
    return true;
}

bool NtpTimeSynchronizer::SelectBestSample(
    const NtpTimeSample* samples,
    size_t sampleCount,
    NtpTimeSample& selected)
{
    if (samples == nullptr)
    {
        return false;
    }

    bool found = false;
    for (size_t index = 0; index < sampleCount; ++index)
    {
        if (!samples[index].isValid ||
            (found && samples[index].roundTripUs >= selected.roundTripUs))
        {
            continue;
        }
        selected = samples[index];
        found = true;
    }
    return found;
}

uint64_t NtpTimeSynchronizer::ExtendTimestampNear(
    uint32_t timestampUs,
    uint64_t referenceTimeUs)
{
    const uint64_t epochBase = referenceTimeUs & ~(TimestampEpochUs - 1U);
    uint64_t selected = epochBase | timestampUs;
    uint64_t selectedDistance = AbsoluteDifference(selected, referenceTimeUs);
    if (selected >= TimestampEpochUs)
    {
        const uint64_t previous = selected - TimestampEpochUs;
        const uint64_t previousDistance = AbsoluteDifference(
            previous,
            referenceTimeUs);
        if (previousDistance < selectedDistance)
        {
            selected = previous;
            selectedDistance = previousDistance;
        }
    }
    if (selected <=
        std::numeric_limits<uint64_t>::max() - TimestampEpochUs)
    {
        const uint64_t next = selected + TimestampEpochUs;
        if (AbsoluteDifference(next, referenceTimeUs) < selectedDistance)
        {
            selected = next;
        }
    }
    return selected;
}

EnTimeQuality NtpTimeSynchronizer::ResolveTimeQuality(
    bool localPowerSaveEnabled,
    bool remotePowerSaveEnabled,
    bool remoteReceiveTimestampAvailable,
    bool localReceiveTimestampAvailable)
{
    if (!remoteReceiveTimestampAvailable ||
        !localReceiveTimestampAvailable)
    {
        return EnTimeQuality::ReceiveTimestampUnavailable;
    }
    if (localPowerSaveEnabled || remotePowerSaveEnabled)
    {
        return EnTimeQuality::PowerSaveEnabled;
    }
    return EnTimeQuality::Synchronized;
}

uint64_t NtpTimeSynchronizer::DefaultTimeProvider()
{
    return static_cast<uint64_t>(esp_timer_get_time());
}

void NtpTimeSynchronizer::DetectMasterChange()
{
    MasterState nextMaster{};
    if (m_coordinator.HasMaster())
    {
        const TagMasterIdentity& identity = m_coordinator.GetMaster();
        if (identity.isValid && identity.sessionId != 0)
        {
            nextMaster.isValid = true;
            nextMaster.isSelfMaster = m_coordinator.IsSelfMaster();
            nextMaster.nodeId = identity.nodeID;
            memcpy(
                nextMaster.macAddress,
                identity.macAddress.data(),
                sizeof(nextMaster.macAddress));
            nextMaster.sessionId = identity.sessionId;
        }
    }
    const bool isSameMaster = m_master.isValid == nextMaster.isValid &&
        (!m_master.isValid ||
         (m_master.isSelfMaster == nextMaster.isSelfMaster &&
          m_master.nodeId == nextMaster.nodeId &&
          m_master.sessionId == nextMaster.sessionId &&
          IsSameMac(m_master.macAddress, nextMaster.macAddress)));
    if (isSameMaster)
    {
        return;
    }

    ResetSynchronizationState();
    m_master = nextMaster;
    if (m_master.isValid && m_master.isSelfMaster)
    {
        DiscoverNewTargets();
    }
}

void NtpTimeSynchronizer::ResetSynchronizationState()
{
    for (TargetState& target : m_targets)
    {
        target = TargetState{};
    }
    m_targetCount = 0;
    m_currentTargetIndex = 0;
    m_pendingResponse = PendingResponse{};
    m_localSynchronization = NodeTimeSynchronization{};
    m_localSynchronizationAnchorUs = 0;
    m_nextSequence = 0;
    m_pendingRequestSequence = 0;
    m_pendingRequestT1 = 0;
    m_requestStartedUs = 0;
    m_requestPending = false;
}

void NtpTimeSynchronizer::DiscoverNewTargets()
{
    if (!m_master.isValid ||
        !m_master.isSelfMaster ||
        m_targetCount >= m_maxTargetCount)
    {
        return;
    }

    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
    const uint32_t nowMs = static_cast<uint32_t>(m_timeProvider() / 1000U);
    uint8_t nodeIdCounts[256]{};
    if (IsValidMac(localStatus.macAddress))
    {
        nodeIdCounts[localStatus.nodeID] = 1;
    }

    const EspNowBroadcast::NodeMap& nodes = m_broadcast.GetNodes();
    for (const auto& node : nodes)
    {
        const NodeStatus& status = node.second;
        uint32_t lastSeenMs = 0;
        if (IsSameMac(status.macAddress, localStatus.macAddress) ||
            !IsValidMac(status.macAddress) ||
            (status.mode != EnRunMode::Anchor &&
             status.mode != EnRunMode::Tag) ||
            !m_broadcast.GetLastSeenMs(node.first, lastSeenMs) ||
            nowMs - lastSeenMs > TagMasterCoordinator::m_nodeExpirationMs)
        {
            continue;
        }
        if (nodeIdCounts[status.nodeID] < UINT8_MAX)
        {
            ++nodeIdCounts[status.nodeID];
        }
    }

    for (size_t nodeId = 0;
         nodeId < 256 && m_targetCount < m_maxTargetCount;
         ++nodeId)
    {
        if (nodeIdCounts[nodeId] != 1)
        {
            continue;
        }

        for (const auto& node : nodes)
        {
            const NodeStatus& status = node.second;
            uint32_t lastSeenMs = 0;
            if (status.nodeID != nodeId ||
                IsSameMac(status.macAddress, localStatus.macAddress) ||
                !IsValidMac(status.macAddress) ||
                (status.mode != EnRunMode::Anchor &&
                 status.mode != EnRunMode::Tag) ||
                !m_broadcast.GetLastSeenMs(node.first, lastSeenMs) ||
                nowMs - lastSeenMs >
                    TagMasterCoordinator::m_nodeExpirationMs ||
                IsTargetTracked(status.nodeID, status.macAddress))
            {
                continue;
            }

            TargetState& target = m_targets[m_targetCount++];
            target.nodeId = status.nodeID;
            memcpy(
                target.macAddress,
                status.macAddress,
                sizeof(target.macAddress));
            break;
        }
    }
}

bool NtpTimeSynchronizer::IsTargetTracked(
    uint8_t nodeId,
    const uint8_t macAddress[6]) const
{
    for (size_t index = 0; index < m_targetCount; ++index)
    {
        if (m_targets[index].nodeId == nodeId ||
            IsSameMac(m_targets[index].macAddress, macAddress))
        {
            return true;
        }
    }
    return false;
}

bool NtpTimeSynchronizer::TranslateClockDomain(
    uint64_t sourceTimeUs,
    uint64_t sourceAnchorUs,
    uint64_t destinationAnchorUs,
    uint64_t& destinationTimeUs)
{
    if (sourceTimeUs >= sourceAnchorUs)
    {
        const uint64_t elapsedUs = sourceTimeUs - sourceAnchorUs;
        if (elapsedUs >
            std::numeric_limits<uint64_t>::max() - destinationAnchorUs)
        {
            return false;
        }
        destinationTimeUs = destinationAnchorUs + elapsedUs;
        return true;
    }

    const uint64_t beforeAnchorUs = sourceAnchorUs - sourceTimeUs;
    if (beforeAnchorUs > destinationAnchorUs)
    {
        return false;
    }
    destinationTimeUs = destinationAnchorUs - beforeAnchorUs;
    return true;
}

void NtpTimeSynchronizer::ProcessReceivedPackets()
{
    EspNowReceivedPacket packet{};
    while (m_transport.PeekReceive(packet))
    {
        if (!NtpTimeProtocolCodec::IsNtpPacket(
                packet.payload,
                packet.payloadLength))
        {
            break;
        }
        if (!m_transport.ConsumeReceive())
        {
            break;
        }

        NtpPacketHeader header{};
        memcpy(&header, packet.payload, sizeof(header));
        switch (static_cast<EnNtpPacketType>(header.packetType))
        {
        case EnNtpPacketType::SyncRequest:
            HandleRequest(packet);
            break;
        case EnNtpPacketType::SyncResponse:
            HandleResponse(packet);
            break;
        case EnNtpPacketType::SyncCommit:
            HandleCommit(packet);
            break;
        }
    }
}

void NtpTimeSynchronizer::HandleRequest(
    const EspNowReceivedPacket& packet)
{
    if (!m_master.isValid ||
        m_master.isSelfMaster ||
        m_pendingResponse.isPending ||
        (packet.hasRxControl &&
         packet.channel != m_configRuntime.GetCurrentEspnowChannel()))
    {
        return;
    }

    NtpSyncRequestPacket request{};
    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
    if (!NtpTimeProtocolCodec::DecodeRequest(
            packet.payload,
            packet.payloadLength,
            request) ||
        request.header.sessionId != m_master.sessionId ||
        request.masterNodeId != m_master.nodeId ||
        request.targetNodeId != localStatus.nodeID ||
        !IsSameMac(request.masterMac, m_master.macAddress) ||
        !IsSameMac(packet.sourceMac, m_master.macAddress) ||
        !IsSameMac(packet.destinationMac, localStatus.macAddress))
    {
        return;
    }

    memcpy(
        m_pendingResponse.destinationMac,
        packet.sourceMac,
        sizeof(m_pendingResponse.destinationMac));
    m_pendingResponse.sessionId = request.header.sessionId;
    m_pendingResponse.sequence = request.header.sequence;
    m_pendingResponse.t1 = request.t1;
    m_pendingResponse.t2 = packet.receivedTimestampUs;
    m_pendingResponse.receiveTimestampAvailable = packet.hasRxControl;
    m_pendingResponse.isPending = true;
}

void NtpTimeSynchronizer::HandleResponse(
    const EspNowReceivedPacket& packet)
{
    if (!m_master.isValid ||
        !m_master.isSelfMaster ||
        !m_requestPending ||
        m_currentTargetIndex >= m_targetCount ||
        (packet.hasRxControl &&
         packet.channel != m_configRuntime.GetCurrentEspnowChannel()))
    {
        return;
    }

    NtpSyncResponsePacket response{};
    TargetState& target = m_targets[m_currentTargetIndex];
    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
    if (!NtpTimeProtocolCodec::DecodeResponse(
            packet.payload,
            packet.payloadLength,
            response) ||
        response.header.sessionId != m_master.sessionId ||
        response.header.sequence != m_pendingRequestSequence ||
        response.targetNodeId != target.nodeId ||
        response.t1 != m_pendingRequestT1 ||
        !IsSameMac(packet.sourceMac, target.macAddress) ||
        !IsSameMac(packet.destinationMac, localStatus.macAddress))
    {
        return;
    }

    NtpTimeSample sample{};
    if (CalculateSample(
            response.t1,
            response.t2,
            response.t3,
            packet.receivedTimestampUs,
            sample))
    {
        sample.rssi = packet.rssi;
        sample.channel = packet.channel;
        sample.timeQuality = ResolveTimeQuality(
            m_configRuntime.GetWifiPowerSave(),
            response.powerSaveEnabled != 0U,
            response.receiveTimestampAvailable != 0U,
            packet.hasRxControl);
        if (target.validSampleCount < m_sampleCountPerNode)
        {
            target.samples[target.validSampleCount++] = sample;
        }
    }
    m_requestPending = false;
}

void NtpTimeSynchronizer::HandleCommit(
    const EspNowReceivedPacket& packet)
{
    if (!m_master.isValid ||
        m_master.isSelfMaster ||
        (packet.hasRxControl &&
         packet.channel != m_configRuntime.GetCurrentEspnowChannel()))
    {
        return;
    }

    NtpSyncCommitPacket commit{};
    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
    if (!NtpTimeProtocolCodec::DecodeCommit(
            packet.payload,
            packet.payloadLength,
            commit) ||
        commit.header.sessionId != m_master.sessionId ||
        commit.targetNodeId != localStatus.nodeID ||
        !IsSameMac(packet.sourceMac, m_master.macAddress) ||
        !IsSameMac(packet.destinationMac, localStatus.macAddress))
    {
        return;
    }

    m_localSynchronization = NodeTimeSynchronization{};
    m_localSynchronization.nodeId = localStatus.nodeID;
    m_localSynchronization.nodeMinusMasterUs = commit.nodeMinusMasterUs;
    m_localSynchronization.roundTripUs = commit.roundTripUs;
    m_localSynchronization.synchronizedAtMasterTimeUs =
        commit.synchronizedAtMasterTimeUs;
    m_localSynchronization.timeQuality = static_cast<EnTimeQuality>(
        commit.timeQuality);
    m_localSynchronization.isValid = true;
    const uint32_t synchronizedAtLocalTimestampUs =
        static_cast<uint32_t>(commit.synchronizedAtMasterTimeUs) +
        static_cast<uint32_t>(commit.nodeMinusMasterUs);
    m_localSynchronizationAnchorUs = ExtendTimestampNear(
        synchronizedAtLocalTimestampUs,
        m_timeProvider());
}

void NtpTimeSynchronizer::TrySendPendingResponse()
{
    if (!m_pendingResponse.isPending ||
        !m_master.isValid ||
        m_master.isSelfMaster ||
        !m_transport.IsSendIdle() ||
        !m_transport.AddPeer(m_pendingResponse.destinationMac))
    {
        return;
    }

    const uint32_t t3 = static_cast<uint32_t>(m_timeProvider());
    NtpSyncResponsePacket response{};
    if (!NtpTimeProtocolCodec::EncodeResponse(
            m_pendingResponse.sessionId,
            m_pendingResponse.sequence,
            m_broadcast.GetLocalStatus().nodeID,
            m_pendingResponse.t1,
            m_pendingResponse.t2,
            t3,
            m_pendingResponse.receiveTimestampAvailable,
            m_configRuntime.GetWifiPowerSave(),
            response) ||
        !m_transport.Send(
            m_pendingResponse.destinationMac,
            reinterpret_cast<const uint8_t*>(&response),
            sizeof(response)))
    {
        return;
    }
    m_pendingResponse = PendingResponse{};
}

void NtpTimeSynchronizer::UpdateMaster()
{
    if (!m_master.isValid || !m_master.isSelfMaster)
    {
        return;
    }
    if (m_requestPending &&
        m_timeProvider() - m_requestStartedUs >= m_responseTimeoutUs)
    {
        m_requestPending = false;
    }
    if (m_requestPending || m_currentTargetIndex >= m_targetCount)
    {
        if (!m_requestPending && m_currentTargetIndex >= m_targetCount)
        {
            DiscoverNewTargets();
        }
        if (m_requestPending || m_currentTargetIndex >= m_targetCount)
        {
            return;
        }
    }

    TargetState& target = m_targets[m_currentTargetIndex];
    if (target.commitPending)
    {
        TrySendCommit();
        return;
    }
    if (target.completed)
    {
        ++m_currentTargetIndex;
        UpdateMaster();
        return;
    }
    if (target.attemptCount >= m_sampleCountPerNode)
    {
        FinalizeCurrentTarget();
        return;
    }
    TrySendRequest();
}

void NtpTimeSynchronizer::TrySendRequest()
{
    if (m_currentTargetIndex >= m_targetCount ||
        !m_transport.IsSendIdle())
    {
        return;
    }

    TargetState& target = m_targets[m_currentTargetIndex];
    if (!m_transport.AddPeer(target.macAddress))
    {
        ++target.attemptCount;
        return;
    }

    const uint32_t sequence = NextSequence();
    const uint64_t t1Full = m_timeProvider();
    const uint32_t t1 = static_cast<uint32_t>(t1Full);
    NtpSyncRequestPacket request{};
    if (!NtpTimeProtocolCodec::EncodeRequest(
            m_master.sessionId,
            sequence,
            m_master.nodeId,
            m_master.macAddress,
            target.nodeId,
            t1,
            request) ||
        !m_transport.Send(
            target.macAddress,
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request)))
    {
        ++target.attemptCount;
        return;
    }

    ++target.attemptCount;
    m_pendingRequestSequence = sequence;
    m_pendingRequestT1 = t1;
    m_requestStartedUs = t1Full;
    m_requestPending = true;
}

void NtpTimeSynchronizer::FinalizeCurrentTarget()
{
    if (m_currentTargetIndex >= m_targetCount)
    {
        return;
    }

    TargetState& target = m_targets[m_currentTargetIndex];
    NtpTimeSample selected{};
    if (SelectBestSample(
            target.samples,
            target.validSampleCount,
            selected))
    {
        target.synchronization.nodeId = target.nodeId;
        target.synchronization.nodeMinusMasterUs =
            selected.nodeMinusMasterUs;
        target.synchronization.roundTripUs = selected.roundTripUs;
        target.synchronization.synchronizedAtMasterTimeUs = m_timeProvider();
        target.synchronization.rssi = selected.rssi;
        target.synchronization.channel = selected.channel;
        target.synchronization.timeQuality = selected.timeQuality;
        target.synchronization.isValid = true;
        target.commitPending = true;
    }
    if (!target.commitPending)
    {
        target.completed = true;
    }
}

void NtpTimeSynchronizer::TrySendCommit()
{
    if (m_currentTargetIndex >= m_targetCount ||
        !m_transport.IsSendIdle())
    {
        return;
    }

    TargetState& target = m_targets[m_currentTargetIndex];
    if (!target.commitPending || !target.synchronization.isValid)
    {
        return;
    }

    const uint64_t committedAtMasterTimeUs = m_timeProvider();
    NtpSyncCommitPacket commit{};
    if (!NtpTimeProtocolCodec::EncodeCommit(
            m_master.sessionId,
            NextSequence(),
            target.nodeId,
            target.synchronization.nodeMinusMasterUs,
            target.synchronization.roundTripUs,
            target.synchronization.timeQuality,
            committedAtMasterTimeUs,
            commit) ||
        !m_transport.Send(
            target.macAddress,
            reinterpret_cast<const uint8_t*>(&commit),
            sizeof(commit)))
    {
        return;
    }

    target.commitPending = false;
    target.completed = true;
}

uint32_t NtpTimeSynchronizer::NextSequence()
{
    ++m_nextSequence;
    if (m_nextSequence == 0)
    {
        ++m_nextSequence;
    }
    return m_nextSequence;
}
