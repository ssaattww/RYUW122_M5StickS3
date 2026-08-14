#pragma once

#include <array>
#include <cstdint>
#include <map>

#include "NodeStatus.h"

class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;

    const NodeMap& GetNodes() const
    {
        return m_nodes;
    }

    bool GetLastSeenMs(const NodeAddress& address, uint32_t& lastSeenMs) const
    {
        const auto found = m_lastSeen.find(address);
        if (found == m_lastSeen.end())
        {
            return false;
        }
        lastSeenMs = found->second;
        return true;
    }

    const NodeStatus& GetLocalStatus() const
    {
        return m_local;
    }

    void SetLocal(const NodeStatus& status)
    {
        m_local = status;
    }

    void AddNode(const NodeStatus& status, uint32_t lastSeenMs = 0)
    {
        NodeAddress address{};
        for (size_t index = 0; index < address.size(); ++index)
        {
            address[index] = status.macAddress[index];
        }
        m_nodes[address] = status;
        m_lastSeen[address] = lastSeenMs;
    }

private:
    NodeStatus m_local{};
    NodeMap m_nodes;
    std::map<NodeAddress, uint32_t> m_lastSeen;
};
