#pragma once

#include "EspNowTransport.h"
#include "NodeStatus.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <map>

/**
 * @brief T-009 native統合テスト用のNodeStatus一覧を保持します。
 */
class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;

    /**
     * @brief 共有受信FIFOを使用するtest用broadcastを生成します。
     *
     * @param transport 共有するESP-NOW transport
     */
    explicit EspNowBroadcast(EspNowTransport& transport)
        : m_transport(transport)
    {
    }

    /**
     * @brief FIFO先頭にあるNodeStatus packetだけを処理します。
     */
    void Update()
    {
        EspNowReceivedPacket packet{};
        while (m_transport.PeekReceive(packet))
        {
            if (!NodeStatusCodec::IsNodeStatusPacket(
                    packet.payload,
                    packet.payloadLength))
            {
                return;
            }
            if (!m_transport.ConsumeReceive())
            {
                return;
            }

            NodeStatus status{};
            if (NodeStatusCodec::Decode(
                    packet.payload,
                    packet.payloadLength,
                    packet.sourceMac,
                    status))
            {
                PutNode(status, 0U);
            }
        }
    }

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
     * @brief remoteノード状態と最終受信時刻を追加または更新します。
     *
     * @param status 追加または更新する状態
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

    /**
     * @brief 自ノードのマスター宣言とセッションIDを更新します。
     *
     * @param isMaster 自ノードがマスターの場合はtrue
     * @param sessionId マスターセッションID
     */
    void SetMasterState(bool isMaster, uint32_t sessionId)
    {
        m_localStatus.isMaster = isMaster;
        m_localStatus.sessionId = isMaster ? sessionId : 0U;
    }

private:
    EspNowTransport& m_transport;
    NodeStatus m_localStatus{};
    NodeMap m_nodes;
    std::map<NodeAddress, uint32_t> m_lastSeen;
};
