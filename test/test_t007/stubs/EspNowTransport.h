#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

struct EspNowReceivedPacket
{
    uint8_t sourceMac[6]{};
    uint8_t destinationMac[6]{};
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t receivedTimestampUs = 0;
    uint16_t payloadLength = 0;
    bool hasRxControl = true;
    uint8_t payload[250]{};
};

struct StubSentPacket
{
    uint8_t destinationMac[6]{};
    uint16_t payloadLength = 0;
    uint8_t payload[250]{};
};

class EspNowTransport
{
public:
    bool AddPeer(const uint8_t[6])
    {
        return m_addPeerSucceeds;
    }

    bool Send(const uint8_t destinationMac[6], const uint8_t* payload,
        size_t payloadLength)
    {
        if (!m_sendSucceeds || m_sentCount >= 128U)
        {
            return false;
        }
        StubSentPacket& packet = m_sent[m_sentCount++];
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.payloadLength = static_cast<uint16_t>(payloadLength);
        memcpy(packet.payload, payload, payloadLength);
        return true;
    }

    bool PeekReceive(EspNowReceivedPacket& packet)
    {
        if (m_receivedCount == 0U)
        {
            return false;
        }
        packet = m_received[m_receivedHead];
        return true;
    }

    bool ConsumeReceive()
    {
        if (m_receivedCount == 0U)
        {
            return false;
        }
        m_receivedHead = (m_receivedHead + 1U) % 128U;
        --m_receivedCount;
        return true;
    }

    bool IsSendIdle() const
    {
        return m_sendIdle;
    }

    void Inject(const EspNowReceivedPacket& packet)
    {
        m_received[m_receivedTail] = packet;
        m_receivedTail = (m_receivedTail + 1U) % 128U;
        ++m_receivedCount;
    }

    bool TakeSent(StubSentPacket& packet)
    {
        if (m_sentRead >= m_sentCount)
        {
            return false;
        }
        packet = m_sent[m_sentRead++];
        return true;
    }

    size_t ReceivedCount() const
    {
        return m_receivedCount;
    }

    bool m_sendIdle = true;
    bool m_sendSucceeds = true;
    bool m_addPeerSucceeds = true;

private:
    EspNowReceivedPacket m_received[128]{};
    size_t m_receivedHead = 0;
    size_t m_receivedTail = 0;
    size_t m_receivedCount = 0;
    StubSentPacket m_sent[128]{};
    size_t m_sentCount = 0;
    size_t m_sentRead = 0;
};
