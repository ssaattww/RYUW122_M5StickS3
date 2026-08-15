#pragma once

#include "NodeStatus.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <map>

/**
 * @brief NTP native test用のNodeStatus一覧を保持します。
 */
class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;

    /**
     * @brief 自ノード状態を設定します。
     *
     * @param status 設定する状態
     */
    void SetLocalStatus(const NodeStatus& status)
    {
        m_localStatus = status;
    }

    /**
     * @brief remoteノード状態と最終受信時刻を追加します。
     *
     * @param status 追加する状態
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
     * @brief remoteノード状態と最終受信時刻を削除します。
     *
     * @param status 削除する状態
     */
    void RemoveNode(const NodeStatus& status)
    {
        NodeAddress address{};
        memcpy(address.data(), status.macAddress, address.size());
        m_nodes.erase(address);
        m_lastSeen.erase(address);
    }

    /**
     * @brief 自ノード状態を取得します。
     *
     * @return 自ノード状態
     */
    const NodeStatus& GetLocalStatus() const
    {
        return m_localStatus;
    }

    /**
     * @brief remoteノード一覧を取得します。
     *
     * @return remoteノード一覧
     */
    const NodeMap& GetNodes() const
    {
        return m_nodes;
    }

    /**
     * @brief 指定ノードの最終受信時刻を取得します。
     *
     * @param address 対象MACアドレス
     * @param lastSeenMs 最終受信時刻格納先
     * @return 状態が存在する場合はtrue
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

private:
    NodeStatus m_localStatus{};
    NodeMap m_nodes;
    std::map<NodeAddress, uint32_t> m_lastSeen;
};
