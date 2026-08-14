#include "TagMasterCoordinator.h"

#include "EspNowBroadcast.h"

#include <esp_system.h>

#include <cstring>

namespace
{
    /**
     * @brief MACアドレスがゼロまたはbroadcastでないか確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return 有効なユニキャストMACアドレスの場合はtrue、それ以外はfalse
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
     * @brief 2つのマスター識別情報が一致するか確認します。
     *
     * @param left 比較する左辺識別情報
     * @param right 比較する右辺識別情報
     * @return 識別情報が一致する場合はtrue、それ以外はfalse
     */
    bool IsSameIdentity(
        const TagMasterIdentity& left,
        const TagMasterIdentity& right)
    {
        return left.isValid == right.isValid &&
            (!left.isValid ||
             (left.nodeID == right.nodeID &&
              left.macAddress == right.macAddress &&
              left.sessionId == right.sessionId));
    }

    /**
     * @brief NodeStatusからマスター識別情報を生成します。
     *
     * @param status 変換するノード状態
     * @param sessionId 適用するセッションID
     * @return 生成したマスター識別情報
     */
    TagMasterIdentity BuildIdentity(
        const NodeStatus& status,
        uint32_t sessionId)
    {
        TagMasterIdentity identity{};
        identity.isValid = true;
        identity.nodeID = status.nodeID;
        memcpy(
            identity.macAddress.data(),
            status.macAddress,
            identity.macAddress.size());
        identity.sessionId = sessionId;
        return identity;
    }
}

TagMasterCoordinator::TagMasterCoordinator(EspNowBroadcast& broadcast)
    : m_broadcast(broadcast)
{
}

void TagMasterCoordinator::Begin(uint32_t nowMs)
{
    m_startedAtMs = nowMs;
    m_started = true;
    m_electionComplete = false;
    m_isSelfMaster = false;
    m_hasPendingChange = false;
    m_master = TagMasterIdentity{};
    m_pendingChange = TagMasterChange{};
    m_broadcast.SetMasterState(false, 0);
}

void TagMasterCoordinator::Update(uint32_t nowMs)
{
    if (!m_started ||
        (!m_electionComplete &&
         nowMs - m_startedAtMs < m_startupElectionWaitMs))
    {
        return;
    }

    m_electionComplete = true;
    MasterCandidate candidate{};
    if (SelectCandidate(nowMs, candidate))
    {
        ApplyCandidate(candidate);
    }
    else
    {
        ClearMaster();
    }
}

bool TagMasterCoordinator::IsElectionComplete() const
{
    return m_electionComplete;
}

bool TagMasterCoordinator::HasMaster() const
{
    return m_master.isValid;
}

bool TagMasterCoordinator::IsSelfMaster() const
{
    return m_isSelfMaster;
}

const TagMasterIdentity& TagMasterCoordinator::GetMaster() const
{
    return m_master;
}

bool TagMasterCoordinator::TryTakeMasterChange(TagMasterChange& change)
{
    if (!m_hasPendingChange)
    {
        return false;
    }
    change = m_pendingChange;
    m_hasPendingChange = false;
    return true;
}

bool TagMasterCoordinator::SelectCandidate(
    uint32_t nowMs,
    MasterCandidate& candidate) const
{
    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
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
            !m_broadcast.GetLastSeenMs(node.first, lastSeenMs) ||
            nowMs - lastSeenMs > m_nodeExpirationMs)
        {
            continue;
        }
        if (nodeIdCounts[status.nodeID] < UINT8_MAX)
        {
            ++nodeIdCounts[status.nodeID];
        }
    }

    bool found = false;
    if (localStatus.mode == EnRunMode::Tag &&
        IsValidMac(localStatus.macAddress) &&
        nodeIdCounts[localStatus.nodeID] == 1)
    {
        candidate.status = localStatus;
        candidate.isSelf = true;
        found = true;
    }

    for (const auto& node : nodes)
    {
        const NodeStatus& status = node.second;
        uint32_t lastSeenMs = 0;
        if (status.mode != EnRunMode::Tag ||
            IsSameMac(status.macAddress, localStatus.macAddress) ||
            !IsValidMac(status.macAddress) ||
            !m_broadcast.GetLastSeenMs(node.first, lastSeenMs) ||
            nowMs - lastSeenMs > m_nodeExpirationMs ||
            nodeIdCounts[status.nodeID] != 1)
        {
            continue;
        }
        if (!found || status.nodeID < candidate.status.nodeID)
        {
            candidate.status = status;
            candidate.isSelf = false;
            found = true;
        }
    }
    return found;
}

void TagMasterCoordinator::ApplyCandidate(
    const MasterCandidate& candidate)
{
    if (!candidate.isSelf &&
        (!candidate.status.isMaster || candidate.status.sessionId == 0))
    {
        ClearMaster();
        return;
    }

    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
    uint32_t sessionId = 0;
    if (candidate.isSelf)
    {
        const bool sameSelfMaster = m_master.isValid &&
            IsSameMac(m_master.macAddress.data(), localStatus.macAddress) &&
            m_master.nodeID == localStatus.nodeID &&
            m_master.sessionId != 0;
        sessionId = sameSelfMaster ? m_master.sessionId : GenerateSessionId();
    }
    else if (candidate.status.isMaster)
    {
        sessionId = candidate.status.sessionId;
    }

    const TagMasterIdentity nextMaster = BuildIdentity(
        candidate.status,
        sessionId);
    const bool nextIsSelfMaster = candidate.isSelf;
    m_broadcast.SetMasterState(nextIsSelfMaster, sessionId);
    m_isSelfMaster = nextIsSelfMaster;
    if (IsSameIdentity(m_master, nextMaster))
    {
        return;
    }

    m_pendingChange.previousMaster = m_master;
    m_pendingChange.currentMaster = nextMaster;
    m_pendingChange.requiresStateReset = true;
    m_master = nextMaster;
    m_hasPendingChange = true;
}

void TagMasterCoordinator::ClearMaster()
{
    m_broadcast.SetMasterState(false, 0);
    m_isSelfMaster = false;
    if (!m_master.isValid)
    {
        return;
    }

    m_pendingChange.previousMaster = m_master;
    m_pendingChange.currentMaster = TagMasterIdentity{};
    m_pendingChange.requiresStateReset = true;
    m_master = TagMasterIdentity{};
    m_hasPendingChange = true;
}

uint32_t TagMasterCoordinator::GenerateSessionId() const
{
    const uint32_t sessionId = esp_random();
    return sessionId == 0 ? 1U : sessionId;
}
