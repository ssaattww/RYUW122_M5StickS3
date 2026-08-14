#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>

/**
 * @brief T-009 native統合テスト用の受信packetを表します。
 */
struct EspNowReceivedPacket
{
    uint8_t sourceMac[6]{};
    uint8_t destinationMac[6]{};
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t receivedTimestampUs = 0;
    uint16_t payloadLength = 0;
    bool hasRxControl = false;
    uint8_t payload[250]{};
};

/**
 * @brief T-009 native統合テスト用の送信packetを表します。
 */
struct EspNowTestSentPacket
{
    uint8_t destinationMac[6]{};
    uint16_t payloadLength = 0;
    uint8_t payload[250]{};
};

/**
 * @brief production状態機械をmemory上で接続するtransportです。
 */
class EspNowTransport
{
public:
    /**
     * @brief peer追加を常に成功させます。
     *
     * @param destinationMac 追加する送信先MACアドレス
     * @return MACアドレスがnullでなければtrue
     */
    bool AddPeer(const uint8_t destinationMac[6])
    {
        return destinationMac != nullptr;
    }

    /**
     * @brief memory上の送信FIFOへpacketを追加します。
     *
     * @param destinationMac 送信先MACアドレス
     * @param payload 送信payload
     * @param payloadLength 送信payloadサイズ
     * @return 入力とサイズが有効な場合はtrue
     */
    bool Send(
        const uint8_t destinationMac[6],
        const uint8_t* payload,
        size_t payloadLength)
    {
        if (destinationMac == nullptr || payload == nullptr ||
            payloadLength == 0U || payloadLength > 250U)
        {
            return false;
        }
        EspNowTestSentPacket packet{};
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.payloadLength = static_cast<uint16_t>(payloadLength);
        memcpy(packet.payload, payload, payloadLength);
        m_sentPackets.push_back(packet);
        return true;
    }

    /**
     * @brief 受信FIFO先頭を削除せず取得します。
     *
     * @param packet 受信packet格納先
     * @return packetが存在する場合はtrue
     */
    bool PeekReceive(EspNowReceivedPacket& packet)
    {
        if (m_receivedPackets.empty())
        {
            return false;
        }
        packet = m_receivedPackets.front();
        return true;
    }

    /**
     * @brief 受信FIFO先頭を削除します。
     *
     * @return packetが存在する場合はtrue
     */
    bool ConsumeReceive()
    {
        if (m_receivedPackets.empty())
        {
            return false;
        }
        m_receivedPackets.pop_front();
        ++m_consumedReceiveCount;
        return true;
    }

    /**
     * @brief 成功した受信FIFO削除の累積件数を取得します。
     *
     * @return ConsumeReceive()が成功した累積件数
     */
    uint32_t GetConsumedReceiveCount() const
    {
        return m_consumedReceiveCount;
    }

    /**
     * @brief 送信可能状態を返します。
     *
     * @return native統合テストでは常にtrue
     */
    bool IsSendIdle() const
    {
        return true;
    }

    /**
     * @brief 受信FIFOへpacketを追加します。
     *
     * @param packet 追加するpacket
     */
    void PushReceived(const EspNowReceivedPacket& packet)
    {
        m_receivedPackets.push_back(packet);
    }

    /**
     * @brief 受信FIFOに残るpacket件数を取得します。
     *
     * @return 受信FIFOのpacket件数
     */
    size_t GetReceivedPacketCount() const
    {
        return m_receivedPackets.size();
    }

    /**
     * @brief 送信FIFO先頭を取得して削除します。
     *
     * @param packet 取得したpacket格納先
     * @return packetが存在する場合はtrue
     */
    bool TakeSent(EspNowTestSentPacket& packet)
    {
        if (m_sentPackets.empty())
        {
            return false;
        }
        packet = m_sentPackets.front();
        m_sentPackets.pop_front();
        return true;
    }

    /**
     * @brief session切替時にtest transport内の未配送packetを破棄します。
     */
    void ClearPackets()
    {
        m_receivedPackets.clear();
        m_sentPackets.clear();
    }

private:
    std::deque<EspNowReceivedPacket> m_receivedPackets;
    std::deque<EspNowTestSentPacket> m_sentPackets;
    uint32_t m_consumedReceiveCount = 0;
};
