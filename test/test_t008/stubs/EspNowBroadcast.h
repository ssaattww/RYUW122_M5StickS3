#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

#include "RunMode.h"

/**
 * @brief test用受信ノード状態を表します。
 */
struct NodeStatus
{
    uint8_t nodeID = 0;
    EnRunMode mode = EnRunMode::Tag;
    uint16_t anchorPositionX = 0;
    uint16_t anchorPositionY = 0;
};

/**
 * @brief test用NodeStatus一覧と通知件数を管理します。
 */
class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;

    /**
     * @brief 保留中のNodeStatus通知を1件取得します。
     *
     * @param status 取得したNodeStatus格納先
     * @return 通知を取得した場合はtrue、それ以外はfalse
     */
    bool TryReceive(NodeStatus& status)
    {
        if (m_pendingCount == 0U)
        {
            return false;
        }
        --m_pendingCount;
        status = m_lastStatus;
        return true;
    }

    /**
     * @brief 受信済みNodeStatus一覧を取得します。
     *
     * @return 受信済みNodeStatus一覧
     */
    const NodeMap& GetNodes() const
    {
        return m_nodes;
    }

    /**
     * @brief test用自ノード状態を取得します。
     *
     * @return 自ノード状態
     */
    const NodeStatus& GetLocalStatus() const
    {
        return m_localStatus;
    }

    /**
     * @brief test用自ノード状態を設定します。
     *
     * @param status 設定する自ノード状態
     */
    void SetLocalStatus(const NodeStatus& status)
    {
        m_localStatus = status;
    }

    /**
     * @brief NodeStatus通知をtest用queueへ追加します。
     *
     * @param status 追加するNodeStatus
     */
    void Inject(const NodeStatus& status)
    {
        m_lastStatus = status;
        ++m_pendingCount;
        NodeAddress address{};
        address[5] = status.nodeID;
        m_nodes[address] = status;
    }

private:
    NodeStatus m_localStatus{};
    NodeMap m_nodes;
    NodeStatus m_lastStatus{};
    size_t m_pendingCount = 0;
};
