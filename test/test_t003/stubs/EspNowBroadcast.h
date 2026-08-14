#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <map>

#include "NodeStatus.h"

/**
 * @brief TagMasterCoordinatorのnative test用NodeStatus保存先です。
 */
class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;

    /**
     * @brief native test用の自ノード状態を設定します。
     *
     * @param status 設定する自ノード状態
     */
    void SetLocalStatus(const NodeStatus& status)
    {
        m_localStatus = status;
    }

    /**
     * @brief native test用の受信ノード状態を追加します。
     *
     * @param status 追加するノード状態
     * @param lastSeenMs 最終受信時刻
     */
    void PutNode(const NodeStatus& status, uint32_t lastSeenMs)
    {
        NodeAddress address{};
        memcpy(address.data(), status.macAddress, address.size());
        m_nodes[address] = status;
        m_lastSeen[address] = lastSeenMs;
    }

    /**
     * @brief native test用に受信ノード一覧を取得します。
     *
     * @return 受信ノード一覧
     */
    const NodeMap& GetNodes() const
    {
        return m_nodes;
    }

    /**
     * @brief native test用に最終受信時刻を取得します。
     *
     * @param address 対象ノードのMACアドレス
     * @param lastSeenMs 最終受信時刻の格納先
     * @return 時刻が存在する場合はtrue、それ以外はfalse
     */
    bool GetLastSeenMs(
        const NodeAddress& address,
        uint32_t& lastSeenMs) const
    {
        const auto found = m_lastSeen.find(address);
        if (found == m_lastSeen.end())
        {
            return false;
        }
        lastSeenMs = found->second;
        return true;
    }

    /**
     * @brief native test用に自ノード状態を取得します。
     *
     * @return 自ノード状態
     */
    const NodeStatus& GetLocalStatus() const
    {
        return m_localStatus;
    }

    /**
     * @brief native test用に自ノードのマスター状態を更新します。
     *
     * @param isMaster 自ノードがマスターの場合はtrue
     * @param sessionId マスターセッションID
     */
    void SetMasterState(bool isMaster, uint32_t sessionId)
    {
        m_localStatus.isMaster = isMaster;
        m_localStatus.sessionId = isMaster ? sessionId : 0;
    }

private:
    NodeStatus m_localStatus{};
    NodeMap m_nodes;
    std::map<NodeAddress, uint32_t> m_lastSeen;
};
