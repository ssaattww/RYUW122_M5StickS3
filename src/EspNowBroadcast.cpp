#include "EspNowBroadcast.h"

#include <Arduino.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>

namespace
{
    constexpr UBaseType_t ReceivedNodeStatusQueueLength = 16;
    constexpr uint32_t StatusSendIntervalMs = 1000;
    constexpr uint8_t BroadcastMac[6] = {
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
    };

    /**
     * @brief MACアドレスが有効なユニキャスト送信元か確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return ゼロまたはbroadcastアドレスでない場合はtrue、それ以外はfalse
     */
    bool IsValidSourceMac(const uint8_t macAddress[6])
    {
        bool allZero = true;
        bool allBroadcast = true;
        for (size_t index = 0; index < 6; ++index)
        {
            allZero = allZero && macAddress[index] == 0U;
            allBroadcast = allBroadcast && macAddress[index] == 0xffU;
        }
        return !allZero && !allBroadcast;
    }

    /**
     * @brief MACアドレスがbroadcast宛先か確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return broadcastアドレスの場合はtrue、それ以外はfalse
     */
    bool IsBroadcastAddress(const uint8_t macAddress[6])
    {
        return memcmp(macAddress, BroadcastMac, sizeof(BroadcastMac)) == 0;
    }
}

EspNowBroadcast::EspNowBroadcast(
    EspNowTransport& transport,
    ConfigRuntime& configRuntime)
    : m_transport(transport),
      m_configRuntime(configRuntime)
{
}

EspNowBroadcast::~EspNowBroadcast()
{
    if (m_receivedNodeStatusQueue != nullptr)
    {
        vQueueDelete(m_receivedNodeStatusQueue);
        m_receivedNodeStatusQueue = nullptr;
    }
}

bool EspNowBroadcast::Begin()
{
    if (m_started)
    {
        return true;
    }
    if (!m_transport.IsStarted())
    {
        return false;
    }
    if (m_receivedNodeStatusQueue == nullptr)
    {
        m_receivedNodeStatusQueue = xQueueCreate(
            ReceivedNodeStatusQueueLength,
            sizeof(NodeStatus));
        if (m_receivedNodeStatusQueue == nullptr)
        {
            return false;
        }
    }
    if (esp_wifi_get_mac(WIFI_IF_STA, m_localStatus.macAddress) != ESP_OK)
    {
        return false;
    }

    RefreshLocalStatus();
    m_started = true;
    m_sendImmediately = true;
    SendNodeStatus();
    return true;
}

void EspNowBroadcast::Update()
{
    if (!m_started)
    {
        return;
    }

    EspNowReceivedPacket packet{};
    while (m_transport.PeekReceive(packet))
    {
        if (!NodeStatusCodec::IsNodeStatusPacket(
                packet.payload,
                packet.payloadLength))
        {
            break;
        }
        if (!m_transport.ConsumeReceive())
        {
            break;
        }
        HandleReceivedPacket(packet);
    }

    if (RefreshLocalStatus())
    {
        m_sendImmediately = true;
    }
    SendNodeStatus();
}

bool EspNowBroadcast::TryReceive(NodeStatus& status)
{
    if (m_receivedNodeStatusQueue == nullptr)
    {
        return false;
    }
    return xQueueReceive(
        m_receivedNodeStatusQueue,
        &status,
        0) == pdTRUE;
}

const EspNowBroadcast::NodeMap& EspNowBroadcast::GetNodes() const
{
    return m_nodes;
}

bool EspNowBroadcast::GetLastSeenMs(
    const NodeAddress& address,
    uint32_t& lastSeenMs) const
{
    const auto lastSeen = m_lastSeenByAddress.find(address);
    if (lastSeen == m_lastSeenByAddress.end())
    {
        return false;
    }
    lastSeenMs = lastSeen->second;
    return true;
}

const NodeStatus& EspNowBroadcast::GetLocalStatus() const
{
    return m_localStatus;
}

void EspNowBroadcast::SetMasterState(bool isMaster, uint32_t sessionId)
{
    if (!isMaster)
    {
        sessionId = 0;
    }
    if (m_localStatus.isMaster == isMaster &&
        m_localStatus.sessionId == sessionId)
    {
        return;
    }

    m_localStatus.isMaster = isMaster;
    m_localStatus.sessionId = sessionId;
    m_sendImmediately = true;
}

bool EspNowBroadcast::IsStarted() const
{
    return m_started && m_transport.IsStarted();
}

void EspNowBroadcast::HandleReceivedPacket(
    const EspNowReceivedPacket& packet)
{
    if (!IsBroadcastAddress(packet.destinationMac) ||
        !IsValidSourceMac(packet.sourceMac))
    {
        return;
    }

    NodeStatus status{};
    if (!NodeStatusCodec::Decode(
            packet.payload,
            packet.payloadLength,
            packet.sourceMac,
            status))
    {
        return;
    }

    const NodeAddress address{
        packet.sourceMac[0],
        packet.sourceMac[1],
        packet.sourceMac[2],
        packet.sourceMac[3],
        packet.sourceMac[4],
        packet.sourceMac[5],
    };
    m_nodes[address] = status;
    m_lastSeenByAddress[address] = millis();
    xQueueSend(m_receivedNodeStatusQueue, &status, 0);
}

bool EspNowBroadcast::RefreshLocalStatus()
{
    NodeStatus refreshed = m_localStatus;
    refreshed.anchorPositionX = m_configRuntime.GetAnchorPositionX();
    refreshed.anchorPositionY = m_configRuntime.GetAnchorPositionY();
    refreshed.nodeID = m_configRuntime.GetCurrentNodeID();
    refreshed.mode = m_configRuntime.GetRunMode();
    BuildUwbAddress(refreshed.uwbAddress);

    if (refreshed.anchorPositionX == m_localStatus.anchorPositionX &&
        refreshed.anchorPositionY == m_localStatus.anchorPositionY &&
        refreshed.nodeID == m_localStatus.nodeID &&
        refreshed.mode == m_localStatus.mode &&
        memcmp(
            refreshed.uwbAddress,
            m_localStatus.uwbAddress,
            sizeof(refreshed.uwbAddress)) == 0)
    {
        return false;
    }

    m_localStatus = refreshed;
    return true;
}

void EspNowBroadcast::BuildUwbAddress(char address[9]) const
{
    const char rolePrefix =
        m_configRuntime.GetRunMode() == EnRunMode::Tag ? 'T' : 'A';
    snprintf(
        address,
        9,
        "%c%07u",
        rolePrefix,
        static_cast<unsigned int>(m_configRuntime.GetCurrentNodeID()));
}

void EspNowBroadcast::SendNodeStatus()
{
    if (!m_started)
    {
        return;
    }

    const uint32_t nowMs = millis();
    if (!m_sendImmediately &&
        nowMs - m_lastStatusSendMs < StatusSendIntervalMs)
    {
        return;
    }

    NodeStatusWirePacket packet{};
    if (!NodeStatusCodec::Encode(m_localStatus, packet) ||
        !m_transport.Send(
            BroadcastMac,
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet)))
    {
        return;
    }

    m_lastStatusSendMs = nowMs;
    m_sendImmediately = false;
}
