#pragma once

#include <cstdint>

#include "NtpTimeProtocolCodec.h"

struct NodeTimeSynchronization
{
    uint8_t nodeId = 0;
    int64_t nodeMinusMasterUs = 0;
    uint32_t roundTripUs = 20;
    uint64_t synchronizedAtMasterTimeUs = 0;
    uint64_t synchronizationAgeUs = 30;
    int8_t rssi = -30;
    uint8_t channel = 1;
    EnTimeQuality timeQuality = EnTimeQuality::Synchronized;
    bool isValid = false;
};

class NtpTimeSynchronizer
{
public:
    bool IsSynchronizationComplete() const
    {
        return m_complete;
    }

    bool TryGetNodeSynchronization(uint8_t nodeId,
        NodeTimeSynchronization& synchronization) const
    {
        if (!m_valid[nodeId])
        {
            return false;
        }
        synchronization = m_sync[nodeId];
        return true;
    }

    bool TryConvertNodeTimeToMaster(uint8_t nodeId, uint32_t localTime,
        uint64_t, uint64_t& masterTime) const
    {
        if (!m_valid[nodeId] || m_conversionFails[nodeId])
        {
            return false;
        }
        masterTime = static_cast<uint64_t>(localTime) -
            static_cast<uint32_t>(m_sync[nodeId].nodeMinusMasterUs);
        return true;
    }

    void SetSynchronized(uint8_t nodeId, bool valid = true)
    {
        m_valid[nodeId] = valid;
        m_sync[nodeId].nodeId = nodeId;
        m_sync[nodeId].isValid = valid;
    }

    bool m_complete = true;
    bool m_conversionFails[256]{};

private:
    bool m_valid[256]{};
    NodeTimeSynchronization m_sync[256]{};
};
