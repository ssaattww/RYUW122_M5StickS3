#include "SequentialRangingController.h"

#include "EspNowBroadcast.h"
#include "EspNowTransport.h"
#include "NodeStatus.h"
#include "NtpTimeSynchronizer.h"
#include "Ryuw122Controller.h"
#include "TagMasterCoordinator.h"

#include <esp_timer.h>

#include <cstring>
#include <limits>

namespace
{
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
     * @brief 32bitシーケンスが以前の値より新しいか確認します。
     *
     * @param sequence 確認する新しい値
     * @param previous 以前に受理した値。0は未受理を表す
     * @return 未受理またはmodulo比較で新しい場合はtrue
     */
    bool IsNewerSequence(uint32_t sequence, uint32_t previous)
    {
        return previous == 0U ||
            static_cast<int32_t>(sequence - previous) > 0;
    }

    /**
     * @brief 64bit値に設定されたbit数を数えます。
     *
     * @param value 確認する値
     * @return 設定済みbit数
     */
    uint8_t CountBits(uint64_t value)
    {
        uint8_t count = 0;
        while (value != 0U)
        {
            value &= value - 1U;
            ++count;
        }
        return count;
    }

    /**
     * @brief 64bitマイクロ秒差を公開用32bit範囲へ収めます。
     *
     * @param later 後の時刻
     * @param earlier 前の時刻
     * @return 0からUINT32_MAXへ飽和した差分
     */
    uint32_t SaturatingDuration(uint64_t later, uint64_t earlier)
    {
        if (later <= earlier)
        {
            return 0U;
        }
        const uint64_t difference = later - earlier;
        return difference > std::numeric_limits<uint32_t>::max()
            ? std::numeric_limits<uint32_t>::max()
            : static_cast<uint32_t>(difference);
    }
}

SequentialRangingController::SequentialRangingController(
    EspNowTransport& transport,
    EspNowBroadcast& broadcast,
    TagMasterCoordinator& coordinator,
    NtpTimeSynchronizer& synchronizer,
    Ryuw122Controller& ryuw122,
    SequentialRangingProtocolCodec& codec,
    SequentialRangingTimeProvider timeProvider)
    : m_transport(transport),
      m_broadcast(broadcast),
      m_coordinator(coordinator),
      m_synchronizer(synchronizer),
      m_ryuw122(ryuw122),
      m_codec(codec),
      m_timeProvider(timeProvider == nullptr ? DefaultTimeProvider : timeProvider)
{
}

void SequentialRangingController::Begin()
{
    ResetSessionState();
    m_master = MasterState{};
    m_begun = true;
    DetectMasterChange();
}

void SequentialRangingController::Update()
{
    if (!m_begun)
    {
        return;
    }
    DetectMasterChange();
    m_ryuw122.Update();
    ProcessReceivedPackets();
    UpdateAnchor();
    UpdateMaster();
    TrySendNextPacket();
}

bool SequentialRangingController::TryTakeMeasurement(
    TimedRangeMeasurement& measurement)
{
    if (m_measurementCount == 0U)
    {
        return false;
    }
    measurement = m_measurementQueue[m_measurementHead];
    m_measurementHead = (m_measurementHead + 1U) % m_measurementQueueCapacity;
    --m_measurementCount;
    return true;
}

bool SequentialRangingController::TryTakeCompletedRound(
    SequentialRangeRoundSummary& summary)
{
    if (m_roundCount == 0U)
    {
        return false;
    }
    summary = m_roundQueue[m_roundHead];
    m_roundHead = (m_roundHead + 1U) % m_roundQueueCapacity;
    --m_roundCount;
    return true;
}

EnSequentialRangingState SequentialRangingController::GetState() const
{
    return m_state;
}

SequentialRangingDiagnostics SequentialRangingController::GetDiagnostics() const
{
    return m_diagnostics;
}

uint64_t SequentialRangingController::DefaultTimeProvider()
{
    return static_cast<uint64_t>(esp_timer_get_time());
}

void SequentialRangingController::DetectMasterChange()
{
    MasterState current{};
    if (m_coordinator.HasMaster())
    {
        const TagMasterIdentity& identity = m_coordinator.GetMaster();
        current.isValid = identity.isValid;
        current.isSelfMaster = m_coordinator.IsSelfMaster();
        current.nodeId = identity.nodeID;
        memcpy(current.macAddress, identity.macAddress.data(), 6);
        current.sessionId = identity.sessionId;
    }
    const bool changed = current.isValid != m_master.isValid ||
        current.isSelfMaster != m_master.isSelfMaster ||
        current.nodeId != m_master.nodeId ||
        current.sessionId != m_master.sessionId ||
        !IsSameMac(current.macAddress, m_master.macAddress);
    if (!changed)
    {
        return;
    }

    ResetSessionState();
    m_master = current;
    const NodeStatus& local = m_broadcast.GetLocalStatus();
    if (!current.isValid)
    {
        m_state = EnSequentialRangingState::WaitingForMaster;
    }
    else if (local.mode == EnRunMode::Anchor)
    {
        m_state = EnSequentialRangingState::AnchorIdle;
    }
    else if (current.isSelfMaster)
    {
        m_state = EnSequentialRangingState::WaitingForSynchronization;
    }
    else
    {
        m_state = EnSequentialRangingState::FollowingMaster;
    }
}

void SequentialRangingController::ResetSessionState()
{
    m_anchorCount = 0;
    m_tagCount = 0;
    m_anchorIndex = 0;
    m_tagIndex = 0;
    m_roundId = 0;
    m_roundStartedUs = 0;
    m_roundDeadlineUs = 0;
    m_receivedMeasurementBits = 0;
    memset(m_lastMeasurementSequence, 0, sizeof(m_lastMeasurementSequence));
    memset(m_lastForwardSequence, 0, sizeof(m_lastForwardSequence));
    m_lastControlRoundId = 0;
    m_lastControlPairSequence = 0;
    m_lastControlPacketSequence = 0;
    m_lastCompleteSequence = 0;
    m_lastCompletedRoundId = 0;
    m_nextPacketSequence = 0;
    m_anchorCommandReceivedUs = 0;
    m_anchorListTruncated = false;
    m_tagListTruncated = false;
    m_measurementHead = 0;
    m_measurementTail = 0;
    m_measurementCount = 0;
    m_roundHead = 0;
    m_roundTail = 0;
    m_roundCount = 0;
    m_highPriorityHead = 0;
    m_highPriorityTail = 0;
    m_highPriorityCount = 0;
    m_lowPriorityHead = 0;
    m_lowPriorityTail = 0;
    m_lowPriorityCount = 0;
}

void SequentialRangingController::ProcessReceivedPackets()
{
    EspNowReceivedPacket packet{};
    while (m_transport.PeekReceive(packet))
    {
        if (!m_codec.IsSequentialRangingPacket(
                packet.payload, packet.payloadLength))
        {
            return;
        }

        SequentialRangingPacketHeader header{};
        if (packet.payloadLength >= sizeof(header))
        {
            memcpy(&header, packet.payload, sizeof(header));
        }
        switch (static_cast<EnSequentialRangingPacketType>(header.packetType))
        {
            case EnSequentialRangingPacketType::RangeControl:
                HandleControl(packet);
                break;
            case EnSequentialRangingPacketType::RangeMeasurement:
                HandleMeasurement(packet);
                break;
            case EnSequentialRangingPacketType::RangeMeasurementForward:
                HandleMeasurementForward(packet);
                break;
            case EnSequentialRangingPacketType::RangeRoundComplete:
                HandleRoundComplete(packet);
                break;
            default:
                ++m_diagnostics.invalidPacketCount;
                break;
        }
        m_transport.ConsumeReceive();
    }
}

void SequentialRangingController::HandleControl(
    const EspNowReceivedPacket& packet)
{
    const NodeStatus& local = m_broadcast.GetLocalStatus();
    uint32_t sessionId = 0;
    uint32_t sequence = 0;
    RangeControlData control{};
    if (local.mode != EnRunMode::Anchor || !m_master.isValid ||
        !m_codec.DecodeControl(packet.payload, packet.payloadLength,
            sessionId, sequence, control) ||
        sessionId != m_master.sessionId ||
        control.masterTagId != m_master.nodeId ||
        !IsSameMac(control.masterMac, m_master.macAddress) ||
        !IsSameMac(packet.destinationMac, local.macAddress) ||
        control.tagIndex != 0U ||
        control.anchorIds[control.anchorIndex] != local.nodeID ||
        !m_synchronizer.IsSynchronizationComplete())
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }

    NodeStatus source{};
    const bool sourceValid = control.anchorIndex == 0U
        ? IsSameMac(packet.sourceMac, m_master.macAddress)
        : TryResolveNode(control.anchorIds[control.anchorIndex - 1U], source) &&
            source.mode == EnRunMode::Anchor &&
            IsSameMac(packet.sourceMac, source.macAddress);
    if (!sourceValid)
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }
    if (control.roundId < m_lastControlRoundId ||
        (control.roundId == m_lastControlRoundId &&
         (!IsNewerSequence(sequence, m_lastControlPacketSequence) ||
          control.pairSequence <= m_lastControlPairSequence)))
    {
        ++m_diagnostics.duplicatePacketCount;
        return;
    }

    RangingNodeIdentity anchors[8]{};
    RangingNodeIdentity tags[8]{};
    for (uint8_t index = 0; index < control.anchorCount; ++index)
    {
        NodeStatus status{};
        if (!TryResolveNode(control.anchorIds[index], status) ||
            status.mode != EnRunMode::Anchor)
        {
            ++m_diagnostics.invalidPacketCount;
            return;
        }
        anchors[index] = BuildIdentity(status);
    }
    for (uint8_t index = 0; index < control.tagCount; ++index)
    {
        NodeStatus status{};
        if (!TryResolveNode(control.tagIds[index], status) ||
            status.mode != EnRunMode::Tag)
        {
            ++m_diagnostics.invalidPacketCount;
            return;
        }
        tags[index] = BuildIdentity(status);
    }
    NodeTimeSynchronization synchronization{};
    if (!m_synchronizer.TryGetNodeSynchronization(
            local.nodeID, synchronization))
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }

    memcpy(m_anchors, anchors, sizeof(m_anchors));
    memcpy(m_tags, tags, sizeof(m_tags));
    m_anchorCount = control.anchorCount;
    m_tagCount = control.tagCount;
    m_anchorIndex = control.anchorIndex;
    m_tagIndex = 0;
    m_roundId = control.roundId;
    m_lastControlRoundId = control.roundId;
    m_lastControlPairSequence = control.pairSequence;
    m_lastControlPacketSequence = sequence;
    m_anchorCommandReceivedUs = packet.receivedTimestampUs;
    m_state = EnSequentialRangingState::AnchorRanging;
    if (!StartCurrentAnchorRanging())
    {
        Ryuw122RangingResult failed{};
        memcpy(failed.tagAddress, m_tags[m_tagIndex].uwbAddress, 9);
        failed.status = EnRyuw122RangingStatus::Failed;
        failed.startedAtUs = m_anchorCommandReceivedUs;
        failed.completedAtUs = static_cast<uint32_t>(m_timeProvider());
        CompleteAnchorMeasurement(failed);
    }
}

void SequentialRangingController::HandleMeasurement(
    const EspNowReceivedPacket& packet)
{
    const NodeStatus& local = m_broadcast.GetLocalStatus();
    uint32_t sessionId = 0;
    uint32_t sequence = 0;
    RangeMeasurementData measurement{};
    if (!m_master.isValid || !m_master.isSelfMaster ||
        m_state != EnSequentialRangingState::RunningRound ||
        !m_codec.DecodeMeasurement(packet.payload, packet.payloadLength,
            EnSequentialRangingPacketType::RangeMeasurement,
            sessionId, sequence, measurement) ||
        sessionId != m_master.sessionId ||
        measurement.roundId != m_roundId ||
        measurement.masterTagId != m_master.nodeId ||
        !IsSameMac(measurement.masterMac, m_master.macAddress) ||
        !IsSameMac(packet.destinationMac, local.macAddress) ||
        measurement.anchorCount != m_anchorCount ||
        measurement.tagCount != m_tagCount ||
        measurement.anchorIndex >= m_anchorCount ||
        measurement.tagIndex >= m_tagCount)
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }
    const uint8_t anchorIndex = measurement.anchorIndex;
    const uint8_t tagIndex = measurement.tagIndex;
    const uint8_t bitIndex = static_cast<uint8_t>(
        anchorIndex * m_tagCount + tagIndex);
    const uint64_t bit = UINT64_C(1) << bitIndex;
    if ((m_receivedMeasurementBits & bit) != 0U ||
        !IsNewerSequence(sequence, m_lastMeasurementSequence[anchorIndex]))
    {
        ++m_diagnostics.duplicatePacketCount;
        return;
    }
    if (!IsSameMac(packet.sourceMac, m_anchors[anchorIndex].macAddress) ||
        memcmp(&measurement.anchor, &m_anchors[anchorIndex],
            sizeof(RangingNodeIdentity)) != 0 ||
        memcmp(&measurement.tag, &m_tags[tagIndex],
            sizeof(RangingNodeIdentity)) != 0)
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }

    m_lastMeasurementSequence[anchorIndex] = sequence;
    measurement.espNowRssi = packet.rssi;
    TimedRangeMeasurement timed{};
    const bool synchronized = ConvertMeasurementToMaster(measurement, timed);
    PushMeasurement(timed);
    if (synchronized && measurement.tag.nodeId != local.nodeID)
    {
        QueueMeasurementForward(measurement);
    }
    m_receivedMeasurementBits |= bit;
    const uint8_t expected = static_cast<uint8_t>(m_anchorCount * m_tagCount);
    const uint64_t expectedBits = expected == 64U
        ? UINT64_MAX
        : (UINT64_C(1) << expected) - 1U;
    if (m_receivedMeasurementBits == expectedBits)
    {
        CompleteMasterRound(false);
    }
}

void SequentialRangingController::HandleMeasurementForward(
    const EspNowReceivedPacket& packet)
{
    const NodeStatus& local = m_broadcast.GetLocalStatus();
    uint32_t sessionId = 0;
    uint32_t sequence = 0;
    RangeMeasurementData measurement{};
    if (local.mode != EnRunMode::Tag || !m_master.isValid ||
        m_master.isSelfMaster ||
        !m_codec.DecodeMeasurement(packet.payload, packet.payloadLength,
            EnSequentialRangingPacketType::RangeMeasurementForward,
            sessionId, sequence, measurement) ||
        sessionId != m_master.sessionId ||
        measurement.masterTagId != m_master.nodeId ||
        !IsSameMac(measurement.masterMac, m_master.macAddress) ||
        !IsSameMac(packet.sourceMac, m_master.macAddress) ||
        !IsSameMac(packet.destinationMac, local.macAddress) ||
        measurement.tag.nodeId != local.nodeID ||
        !IsSameMac(measurement.tag.macAddress, local.macAddress) ||
        measurement.tagIndex >= 8U)
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }
    if (measurement.roundId < m_roundId)
    {
        ++m_diagnostics.duplicatePacketCount;
        return;
    }
    if (measurement.roundId > m_roundId)
    {
        m_roundId = measurement.roundId;
        memset(m_lastForwardSequence, 0, sizeof(m_lastForwardSequence));
        m_receivedMeasurementBits = 0;
    }
    const uint8_t anchorIndex = measurement.anchorIndex;
    const uint8_t bitIndex = static_cast<uint8_t>(
        anchorIndex * measurement.tagCount + measurement.tagIndex);
    const uint64_t bit = UINT64_C(1) << bitIndex;
    if ((m_receivedMeasurementBits & bit) != 0U ||
        !IsNewerSequence(sequence, m_lastForwardSequence[anchorIndex]))
    {
        ++m_diagnostics.duplicatePacketCount;
        return;
    }
    m_lastForwardSequence[anchorIndex] = sequence;
    m_receivedMeasurementBits |= bit;
    TimedRangeMeasurement timed{};
    timed.sessionId = sessionId;
    timed.roundId = measurement.roundId;
    timed.anchorId = measurement.anchor.nodeId;
    timed.tagId = measurement.tag.nodeId;
    timed.status = measurement.status;
    timed.distanceMm = measurement.distanceMm;
    timed.uwbRssi = measurement.uwbRssi;
    timed.espNowRssi = measurement.espNowRssi;
    timed.commandReceivedMasterTimeUs =
        measurement.commandReceivedMasterTimeUs;
    timed.rangingStartedMasterTimeUs =
        measurement.rangingStartedMasterTimeUs;
    timed.rangingCompletedMasterTimeUs =
        measurement.rangingCompletedMasterTimeUs;
    timed.rangingDurationUs = static_cast<uint32_t>(
        measurement.rangingCompletedUs - measurement.rangingStartedUs);
    timed.synchronizationRoundTripUs =
        measurement.synchronizationRoundTripUs;
    timed.synchronizationAgeUs = measurement.synchronizationAgeUs;
    timed.timeQuality = measurement.timeQuality;
    timed.isLastMeasurement = measurement.isLastMeasurement;
    PushMeasurement(timed);
}

void SequentialRangingController::HandleRoundComplete(
    const EspNowReceivedPacket& packet)
{
    const NodeStatus& local = m_broadcast.GetLocalStatus();
    uint32_t sessionId = 0;
    uint32_t sequence = 0;
    RangeRoundCompleteData complete{};
    if (!m_master.isValid ||
        !m_codec.DecodeRoundComplete(packet.payload, packet.payloadLength,
            sessionId, sequence, complete) ||
        sessionId != m_master.sessionId ||
        complete.masterTagId != m_master.nodeId ||
        !IsSameMac(complete.masterMac, m_master.macAddress) ||
        !IsSameMac(packet.destinationMac, local.macAddress))
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }
    if (m_master.isSelfMaster)
    {
        if (m_state != EnSequentialRangingState::RunningRound ||
            complete.roundId != m_roundId || m_anchorCount == 0U ||
            complete.nextRoundId !=
                (m_roundId == UINT32_MAX ? 1U : m_roundId + 1U) ||
            complete.anchorCount != m_anchorCount ||
            complete.tagCount != m_tagCount ||
            complete.expectedMeasurementCount !=
                static_cast<uint8_t>(m_anchorCount * m_tagCount) ||
            !IsSameMac(packet.sourceMac,
                m_anchors[m_anchorCount - 1U].macAddress))
        {
            ++m_diagnostics.invalidPacketCount;
            return;
        }
        CompleteMasterRound(
            CountBits(m_receivedMeasurementBits) <
                static_cast<uint8_t>(m_anchorCount * m_tagCount));
        return;
    }
    if (local.mode != EnRunMode::Tag ||
        !IsSameMac(packet.sourceMac, m_master.macAddress) ||
        complete.roundId < m_roundId ||
        complete.roundId <= m_lastCompletedRoundId ||
        !IsNewerSequence(sequence, m_lastCompleteSequence))
    {
        ++m_diagnostics.invalidPacketCount;
        return;
    }
    m_lastCompleteSequence = sequence;
    m_lastCompletedRoundId = complete.roundId;
    if (complete.roundId > m_roundId)
    {
        m_roundId = complete.roundId;
    }
    SequentialRangeRoundSummary summary{};
    summary.sessionId = sessionId;
    summary.roundId = complete.roundId;
    summary.startedMasterTimeUs = complete.startedMasterTimeUs;
    summary.completedMasterTimeUs = complete.completedMasterTimeUs;
    summary.totalDurationUs = SaturatingDuration(
        complete.completedMasterTimeUs, complete.startedMasterTimeUs);
    summary.anchorCount = complete.anchorCount;
    summary.tagCount = complete.tagCount;
    summary.expectedMeasurementCount = complete.expectedMeasurementCount;
    summary.receivedMeasurementCount = complete.receivedMeasurementCount;
    summary.anchorListTruncated = complete.anchorListTruncated;
    summary.tagListTruncated = complete.tagListTruncated;
    summary.timedOut = complete.timedOut;
    PushRoundSummary(summary);
}

void SequentialRangingController::UpdateMaster()
{
    if (!m_master.isValid || !m_master.isSelfMaster ||
        m_broadcast.GetLocalStatus().mode != EnRunMode::Tag)
    {
        return;
    }
    if (m_state != EnSequentialRangingState::RunningRound)
    {
        if (!m_synchronizer.IsSynchronizationComplete())
        {
            m_state = EnSequentialRangingState::WaitingForSynchronization;
            return;
        }
        m_state = EnSequentialRangingState::ReadyToStart;
        StartMasterRound();
        return;
    }
    if (m_timeProvider() >= m_roundDeadlineUs)
    {
        CompleteMasterRound(true);
    }
}

void SequentialRangingController::UpdateAnchor()
{
    Ryuw122RangingResult result{};
    if (m_state != EnSequentialRangingState::AnchorRanging)
    {
        m_ryuw122.TryTakeResult(result);
        return;
    }
    if (m_ryuw122.TryTakeResult(result))
    {
        CompleteAnchorMeasurement(result);
    }
}

bool SequentialRangingController::BuildRoundSnapshot()
{
    m_anchorCount = 0;
    m_tagCount = 0;
    m_anchorListTruncated = false;
    m_tagListTruncated = false;
    for (uint16_t nodeId = 0; nodeId <= UINT8_MAX; ++nodeId)
    {
        NodeStatus status{};
        if (!TryResolveNode(static_cast<uint8_t>(nodeId), status))
        {
            continue;
        }
        NodeTimeSynchronization synchronization{};
        const bool isLocalMaster = status.mode == EnRunMode::Tag &&
            status.nodeID == m_master.nodeId &&
            IsSameMac(status.macAddress, m_master.macAddress);
        if (!isLocalMaster &&
            !m_synchronizer.TryGetNodeSynchronization(
                status.nodeID, synchronization))
        {
            continue;
        }
        if (status.mode == EnRunMode::Anchor)
        {
            if (m_anchorCount < 8U)
            {
                m_anchors[m_anchorCount++] = BuildIdentity(status);
            }
            else
            {
                m_anchorListTruncated = true;
            }
        }
        else
        {
            if (m_tagCount < 8U)
            {
                m_tags[m_tagCount++] = BuildIdentity(status);
            }
            else
            {
                m_tagListTruncated = true;
            }
        }
    }
    return m_anchorCount > 0U && m_tagCount > 0U;
}

bool SequentialRangingController::StartMasterRound()
{
    if (!BuildRoundSnapshot())
    {
        m_state = EnSequentialRangingState::ReadyToStart;
        return false;
    }
    ++m_roundId;
    if (m_roundId == 0U)
    {
        ++m_roundId;
    }
    m_anchorIndex = 0;
    m_tagIndex = 0;
    m_receivedMeasurementBits = 0;
    memset(m_lastMeasurementSequence, 0, sizeof(m_lastMeasurementSequence));
    m_roundStartedUs = m_timeProvider();
    const uint64_t pairBudget =
        static_cast<uint64_t>(m_anchorCount) * m_tagCount * m_uwbTimeoutUs;
    const uint64_t hopBudget =
        static_cast<uint64_t>(m_anchorCount) * m_espNowHopBudgetUs;
    m_roundDeadlineUs = m_roundStartedUs + pairBudget + hopBudget +
        m_finalReturnBudgetUs;
    if (!QueueControl(0U))
    {
        m_state = EnSequentialRangingState::ReadyToStart;
        return false;
    }
    m_state = EnSequentialRangingState::RunningRound;
    return true;
}

void SequentialRangingController::CompleteMasterRound(bool timedOut)
{
    if (m_state != EnSequentialRangingState::RunningRound)
    {
        return;
    }
    const uint64_t completedUs = m_timeProvider();
    const uint8_t expected = static_cast<uint8_t>(m_anchorCount * m_tagCount);
    const uint64_t expectedBits = expected == 64U
        ? UINT64_MAX
        : (UINT64_C(1) << expected) - 1U;
    const uint64_t missingBits = expectedBits & ~m_receivedMeasurementBits;
    const uint8_t received = CountBits(m_receivedMeasurementBits & expectedBits);
    SequentialRangeRoundSummary summary{};
    summary.sessionId = m_master.sessionId;
    summary.roundId = m_roundId;
    summary.startedMasterTimeUs = m_roundStartedUs;
    summary.completedMasterTimeUs = completedUs;
    summary.totalDurationUs = SaturatingDuration(completedUs, m_roundStartedUs);
    summary.anchorCount = m_anchorCount;
    summary.tagCount = m_tagCount;
    summary.expectedMeasurementCount = expected;
    summary.receivedMeasurementCount = received;
    summary.anchorListTruncated = m_anchorListTruncated;
    summary.tagListTruncated = m_tagListTruncated;
    summary.timedOut = timedOut || missingBits != 0U;
    PushRoundSummary(summary);

    uint32_t nextRoundId = m_roundId + 1U;
    if (nextRoundId == 0U)
    {
        nextRoundId = 1U;
    }
    RangeRoundCompleteData complete{};
    complete.roundId = m_roundId;
    complete.nextRoundId = nextRoundId;
    complete.masterTagId = m_master.nodeId;
    memcpy(complete.masterMac, m_master.macAddress, 6);
    complete.startedMasterTimeUs = m_roundStartedUs;
    complete.completedMasterTimeUs = completedUs;
    complete.anchorCount = m_anchorCount;
    complete.tagCount = m_tagCount;
    complete.expectedMeasurementCount = expected;
    complete.receivedMeasurementCount = received;
    complete.missingMeasurementBits = missingBits;
    complete.anchorListTruncated = m_anchorListTruncated;
    complete.tagListTruncated = m_tagListTruncated;
    complete.timedOut = missingBits != 0U;
    QueueRoundCompleteForFollowers(complete);
    m_state = EnSequentialRangingState::ReadyToStart;
    if (m_synchronizer.IsSynchronizationComplete())
    {
        StartMasterRound();
    }
    else
    {
        m_state = EnSequentialRangingState::WaitingForSynchronization;
    }
}

bool SequentialRangingController::StartCurrentAnchorRanging()
{
    m_anchorCommandReceivedUs = static_cast<uint32_t>(
        m_anchorCommandReceivedUs == 0U
            ? m_timeProvider()
            : m_anchorCommandReceivedUs);
    return m_ryuw122.StartRanging(m_tags[m_tagIndex].uwbAddress);
}

void SequentialRangingController::CompleteAnchorMeasurement(
    const Ryuw122RangingResult& result)
{
    RangeMeasurementData measurement{};
    measurement.roundId = m_roundId;
    measurement.pairSequence = static_cast<uint16_t>(
        static_cast<uint16_t>(m_anchorIndex) * m_tagCount + m_tagIndex + 1U);
    measurement.masterTagId = m_master.nodeId;
    memcpy(measurement.masterMac, m_master.macAddress, 6);
    measurement.anchorCount = m_anchorCount;
    measurement.tagCount = m_tagCount;
    measurement.anchorIndex = m_anchorIndex;
    measurement.tagIndex = m_tagIndex;
    measurement.anchor = m_anchors[m_anchorIndex];
    measurement.tag = m_tags[m_tagIndex];
    switch (result.status)
    {
        case EnRyuw122RangingStatus::Success:
            measurement.status = EnRangeResultStatus::Success;
            break;
        case EnRyuw122RangingStatus::TimedOut:
            measurement.status = EnRangeResultStatus::TimedOut;
            break;
        default:
            measurement.status = EnRangeResultStatus::Failed;
            break;
    }
    measurement.distanceMm = measurement.status == EnRangeResultStatus::Success
        ? result.distanceMm
        : 0U;
    measurement.uwbRssi = measurement.status == EnRangeResultStatus::Success
        ? result.uwbRssi
        : 0;
    measurement.commandReceivedUs = m_anchorCommandReceivedUs;
    measurement.rangingStartedUs = result.startedAtUs;
    measurement.rangingCompletedUs = result.completedAtUs;
    measurement.isLastMeasurement =
        m_anchorIndex == static_cast<uint8_t>(m_anchorCount - 1U) &&
        m_tagIndex == static_cast<uint8_t>(m_tagCount - 1U);

    const bool hasNextTag = m_tagIndex + 1U < m_tagCount;
    const bool hasNextAnchor = !hasNextTag &&
        m_anchorIndex + 1U < m_anchorCount;
    if (hasNextAnchor)
    {
        QueueControl(static_cast<uint8_t>(m_anchorIndex + 1U));
    }
    QueueAnchorMeasurement(measurement);
    if (measurement.isLastMeasurement)
    {
        QueueAnchorRoundComplete();
    }
    if (hasNextTag)
    {
        ++m_tagIndex;
        m_anchorCommandReceivedUs = static_cast<uint32_t>(m_timeProvider());
        if (!StartCurrentAnchorRanging())
        {
            Ryuw122RangingResult failed{};
            memcpy(failed.tagAddress, m_tags[m_tagIndex].uwbAddress, 9);
            failed.status = EnRyuw122RangingStatus::Failed;
            failed.startedAtUs = m_anchorCommandReceivedUs;
            failed.completedAtUs = static_cast<uint32_t>(m_timeProvider());
            CompleteAnchorMeasurement(failed);
        }
        return;
    }
    m_state = EnSequentialRangingState::AnchorIdle;
}

bool SequentialRangingController::QueueControl(uint8_t anchorIndex)
{
    RangeControlData control{};
    control.roundId = m_roundId;
    control.pairSequence = static_cast<uint16_t>(
        static_cast<uint16_t>(anchorIndex) * m_tagCount + 1U);
    control.masterTagId = m_master.nodeId;
    memcpy(control.masterMac, m_master.macAddress, 6);
    control.anchorCount = m_anchorCount;
    control.tagCount = m_tagCount;
    control.anchorIndex = anchorIndex;
    control.tagIndex = 0;
    for (uint8_t index = 0; index < m_anchorCount; ++index)
    {
        control.anchorIds[index] = m_anchors[index].nodeId;
    }
    for (uint8_t index = 0; index < m_tagCount; ++index)
    {
        control.tagIds[index] = m_tags[index].nodeId;
    }
    RangeControlPacket packet{};
    if (!m_codec.EncodeControl(m_master.sessionId,
            NextPacketSequence(), control, packet))
    {
        return false;
    }
    return PushOutbound(m_highPriorityQueue, m_highPriorityQueueCapacity,
        m_highPriorityTail, m_highPriorityCount,
        m_anchors[anchorIndex].macAddress, &packet, sizeof(packet));
}

bool SequentialRangingController::QueueAnchorMeasurement(
    const RangeMeasurementData& measurement)
{
    RangeMeasurementPacket packet{};
    if (!m_codec.EncodeMeasurement(m_master.sessionId,
            NextPacketSequence(), measurement, packet))
    {
        return false;
    }
    return PushOutbound(m_lowPriorityQueue, m_lowPriorityQueueCapacity,
        m_lowPriorityTail, m_lowPriorityCount,
        m_master.macAddress, &packet, sizeof(packet));
}

bool SequentialRangingController::QueueAnchorRoundComplete()
{
    RangeRoundCompleteData complete{};
    complete.roundId = m_roundId;
    complete.nextRoundId = m_roundId == UINT32_MAX ? 1U : m_roundId + 1U;
    complete.masterTagId = m_master.nodeId;
    memcpy(complete.masterMac, m_master.macAddress, 6);
    complete.anchorCount = m_anchorCount;
    complete.tagCount = m_tagCount;
    complete.expectedMeasurementCount = static_cast<uint8_t>(
        m_anchorCount * m_tagCount);
    complete.receivedMeasurementCount = complete.expectedMeasurementCount;
    RangeRoundCompletePacket packet{};
    if (!m_codec.EncodeRoundComplete(m_master.sessionId,
            NextPacketSequence(), complete, packet))
    {
        return false;
    }
    return PushOutbound(m_lowPriorityQueue, m_lowPriorityQueueCapacity,
        m_lowPriorityTail, m_lowPriorityCount,
        m_master.macAddress, &packet, sizeof(packet));
}

bool SequentialRangingController::QueueMeasurementForward(
    const RangeMeasurementData& measurement)
{
    RangeMeasurementPacket packet{};
    if (!m_codec.EncodeMeasurementForward(m_master.sessionId,
            NextPacketSequence(), measurement, packet))
    {
        return false;
    }
    return PushOutbound(m_lowPriorityQueue, m_lowPriorityQueueCapacity,
        m_lowPriorityTail, m_lowPriorityCount,
        measurement.tag.macAddress, &packet, sizeof(packet));
}

void SequentialRangingController::QueueRoundCompleteForFollowers(
    const RangeRoundCompleteData& complete)
{
    for (uint8_t index = 0; index < m_tagCount; ++index)
    {
        if (m_tags[index].nodeId == m_master.nodeId)
        {
            continue;
        }
        RangeRoundCompletePacket packet{};
        if (!m_codec.EncodeRoundComplete(m_master.sessionId,
                NextPacketSequence(), complete, packet))
        {
            ++m_diagnostics.invalidPacketCount;
            return;
        }
        PushOutbound(m_lowPriorityQueue, m_lowPriorityQueueCapacity,
            m_lowPriorityTail, m_lowPriorityCount,
            m_tags[index].macAddress, &packet, sizeof(packet));
    }
}

void SequentialRangingController::TrySendNextPacket()
{
    if (!m_transport.IsSendIdle())
    {
        return;
    }
    OutboundPacket* queue = nullptr;
    size_t* head = nullptr;
    size_t* count = nullptr;
    size_t capacity = 0;
    if (m_highPriorityCount > 0U)
    {
        queue = m_highPriorityQueue;
        head = &m_highPriorityHead;
        count = &m_highPriorityCount;
        capacity = m_highPriorityQueueCapacity;
    }
    else if (m_lowPriorityCount > 0U)
    {
        queue = m_lowPriorityQueue;
        head = &m_lowPriorityHead;
        count = &m_lowPriorityCount;
        capacity = m_lowPriorityQueueCapacity;
    }
    else
    {
        return;
    }
    OutboundPacket& packet = queue[*head];
    if (!m_transport.AddPeer(packet.destinationMac) ||
        !m_transport.Send(packet.destinationMac,
            packet.payload, packet.payloadLength))
    {
        ++m_diagnostics.sendFailureCount;
        return;
    }
    *head = (*head + 1U) % capacity;
    --(*count);
}

bool SequentialRangingController::PushOutbound(
    OutboundPacket* queue,
    size_t capacity,
    size_t& tail,
    size_t& count,
    const uint8_t destinationMac[6],
    const void* payload,
    size_t payloadLength)
{
    if (count >= capacity || payloadLength > sizeof(OutboundPacket::payload))
    {
        ++m_diagnostics.outboundQueueOverflowCount;
        return false;
    }
    OutboundPacket& packet = queue[tail];
    packet = OutboundPacket{};
    memcpy(packet.destinationMac, destinationMac, 6);
    packet.payloadLength = static_cast<uint16_t>(payloadLength);
    memcpy(packet.payload, payload, payloadLength);
    tail = (tail + 1U) % capacity;
    ++count;
    return true;
}

bool SequentialRangingController::ConvertMeasurementToMaster(
    RangeMeasurementData& measurement,
    TimedRangeMeasurement& timedMeasurement)
{
    timedMeasurement.sessionId = m_master.sessionId;
    timedMeasurement.roundId = measurement.roundId;
    timedMeasurement.anchorId = measurement.anchor.nodeId;
    timedMeasurement.tagId = measurement.tag.nodeId;
    timedMeasurement.status = measurement.status;
    timedMeasurement.distanceMm = measurement.distanceMm;
    timedMeasurement.uwbRssi = measurement.uwbRssi;
    timedMeasurement.espNowRssi = measurement.espNowRssi;
    timedMeasurement.rangingDurationUs = static_cast<uint32_t>(
        measurement.rangingCompletedUs - measurement.rangingStartedUs);
    timedMeasurement.isLastMeasurement = measurement.isLastMeasurement;

    NodeTimeSynchronization synchronization{};
    const uint64_t referenceUs = m_timeProvider();
    const bool synchronized =
        m_synchronizer.TryGetNodeSynchronization(
            measurement.anchor.nodeId, synchronization) &&
        m_synchronizer.TryConvertNodeTimeToMaster(
            measurement.anchor.nodeId, measurement.commandReceivedUs,
            referenceUs, measurement.commandReceivedMasterTimeUs) &&
        m_synchronizer.TryConvertNodeTimeToMaster(
            measurement.anchor.nodeId, measurement.rangingStartedUs,
            referenceUs, measurement.rangingStartedMasterTimeUs) &&
        m_synchronizer.TryConvertNodeTimeToMaster(
            measurement.anchor.nodeId, measurement.rangingCompletedUs,
            referenceUs, measurement.rangingCompletedMasterTimeUs);
    if (!synchronized)
    {
        measurement.commandReceivedMasterTimeUs = 0;
        measurement.rangingStartedMasterTimeUs = 0;
        measurement.rangingCompletedMasterTimeUs = 0;
        measurement.synchronizationRoundTripUs = 0;
        measurement.synchronizationAgeUs = 0;
        measurement.timeQuality = EnTimeQuality::Unsynchronized;
        timedMeasurement.timeQuality = EnTimeQuality::Unsynchronized;
        return false;
    }
    measurement.synchronizationRoundTripUs = synchronization.roundTripUs;
    measurement.synchronizationAgeUs = synchronization.synchronizationAgeUs;
    measurement.timeQuality = synchronization.timeQuality;
    timedMeasurement.commandReceivedMasterTimeUs =
        measurement.commandReceivedMasterTimeUs;
    timedMeasurement.rangingStartedMasterTimeUs =
        measurement.rangingStartedMasterTimeUs;
    timedMeasurement.rangingCompletedMasterTimeUs =
        measurement.rangingCompletedMasterTimeUs;
    timedMeasurement.synchronizationRoundTripUs = synchronization.roundTripUs;
    timedMeasurement.synchronizationAgeUs = synchronization.synchronizationAgeUs;
    timedMeasurement.timeQuality = synchronization.timeQuality;
    return true;
}

bool SequentialRangingController::PushMeasurement(
    const TimedRangeMeasurement& measurement)
{
    if (m_measurementCount >= m_measurementQueueCapacity)
    {
        ++m_diagnostics.measurementQueueOverflowCount;
        return false;
    }
    m_measurementQueue[m_measurementTail] = measurement;
    m_measurementTail = (m_measurementTail + 1U) % m_measurementQueueCapacity;
    ++m_measurementCount;
    return true;
}

bool SequentialRangingController::PushRoundSummary(
    const SequentialRangeRoundSummary& summary)
{
    if (m_roundCount >= m_roundQueueCapacity)
    {
        ++m_diagnostics.roundQueueOverflowCount;
        return false;
    }
    m_roundQueue[m_roundTail] = summary;
    m_roundTail = (m_roundTail + 1U) % m_roundQueueCapacity;
    ++m_roundCount;
    return true;
}

bool SequentialRangingController::TryResolveNode(
    uint8_t nodeId, NodeStatus& status) const
{
    size_t matchCount = 0;
    const NodeStatus& local = m_broadcast.GetLocalStatus();
    if (local.nodeID == nodeId)
    {
        status = local;
        ++matchCount;
    }
    const uint32_t nowMs = static_cast<uint32_t>(m_timeProvider() / 1000U);
    for (const auto& entry : m_broadcast.GetNodes())
    {
        uint32_t lastSeenMs = 0;
        if (entry.second.nodeID != nodeId ||
            !m_broadcast.GetLastSeenMs(entry.first, lastSeenMs) ||
            nowMs - lastSeenMs > TagMasterCoordinator::m_nodeExpirationMs)
        {
            continue;
        }
        status = entry.second;
        ++matchCount;
    }
    return matchCount == 1U;
}

RangingNodeIdentity SequentialRangingController::BuildIdentity(
    const NodeStatus& status)
{
    RangingNodeIdentity identity{};
    identity.nodeId = status.nodeID;
    memcpy(identity.macAddress, status.macAddress, 6);
    memcpy(identity.uwbAddress, status.uwbAddress, 9);
    return identity;
}

uint32_t SequentialRangingController::NextPacketSequence()
{
    ++m_nextPacketSequence;
    if (m_nextPacketSequence == 0U)
    {
        ++m_nextPacketSequence;
    }
    return m_nextPacketSequence;
}
