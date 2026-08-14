#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

/**
 * @brief NTP native test用の受信packetを表します。
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
 * @brief NTP native test用の送信packetを表します。
 */
struct EspNowTestSentPacket
{
    uint8_t destinationMac[6]{};
    std::vector<uint8_t> payload;
};

/**
 * @brief NTP状態機械をmemory上で駆動するtransportです。
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
     * @brief memory上の送信履歴へpacketを追加します。
     *
     * @param destinationMac 送信先MACアドレス
     * @param payload 送信payload
     * @param payloadLength 送信payloadサイズ
     * @return 入力が有効な場合はtrue
     */
    bool Send(
        const uint8_t destinationMac[6],
        const uint8_t* payload,
        size_t payloadLength)
    {
        if (destinationMac == nullptr ||
            payload == nullptr ||
            payloadLength == 0 ||
            !m_sendIdle)
        {
            return false;
        }
        EspNowTestSentPacket packet{};
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.payload.assign(payload, payload + payloadLength);
        m_sentPackets.push_back(packet);
        return true;
    }

    /**
     * @brief 受信FIFOの先頭packetを削除せず取得します。
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
     * @brief 受信FIFOの先頭packetを削除します。
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
        return true;
    }

    /**
     * @brief 送信待ちがないか確認します。
     *
     * @return test設定がidleの場合はtrue
     */
    bool IsSendIdle() const
    {
        return m_sendIdle;
    }

    /**
     * @brief test用受信FIFOへpacketを追加します。
     *
     * @param packet 追加する受信packet
     */
    void PushReceived(const EspNowReceivedPacket& packet)
    {
        m_receivedPackets.push_back(packet);
    }

    /**
     * @brief test用受信FIFOの件数を取得します。
     *
     * @return FIFO内packet件数
     */
    size_t GetReceiveCount() const
    {
        return m_receivedPackets.size();
    }

    /**
     * @brief 送信履歴を取得します。
     *
     * @return 送信packet一覧
     */
    const std::vector<EspNowTestSentPacket>& GetSentPackets() const
    {
        return m_sentPackets;
    }

    /**
     * @brief 送信idle状態を設定します。
     *
     * @param idle idleにする場合はtrue
     */
    void SetSendIdle(bool idle)
    {
        m_sendIdle = idle;
    }

private:
    std::deque<EspNowReceivedPacket> m_receivedPackets;
    std::vector<EspNowTestSentPacket> m_sentPackets;
    bool m_sendIdle = true;
};
