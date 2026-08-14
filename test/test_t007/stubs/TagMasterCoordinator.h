#pragma once

#include <array>
#include <cstdint>

struct TagMasterIdentity
{
    bool isValid = false;
    uint8_t nodeID = 0;
    std::array<uint8_t, 6> macAddress{};
    uint32_t sessionId = 0;
};

class TagMasterCoordinator
{
public:
    static constexpr uint32_t m_nodeExpirationMs = 30000;

    bool HasMaster() const
    {
        return m_master.isValid;
    }

    bool IsSelfMaster() const
    {
        return m_selfMaster;
    }

    const TagMasterIdentity& GetMaster() const
    {
        return m_master;
    }

    void SetMaster(const TagMasterIdentity& master, bool selfMaster)
    {
        m_master = master;
        m_selfMaster = selfMaster;
    }

private:
    TagMasterIdentity m_master{};
    bool m_selfMaster = false;
};
