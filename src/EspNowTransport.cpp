#include "EspNowTransport.h"

#include <WiFi.h>
#include <esp_timer.h>
#include <esp_wifi.h>

#include <cstring>

namespace
{
    constexpr UBaseType_t ReceivedQueueLength = 16;
    constexpr UBaseType_t SendQueueLength = 16;
    constexpr UBaseType_t SendCallbackQueueLength = 4;
    constexpr UBaseType_t SendResultQueueLength = 16;
    constexpr uint8_t BroadcastMac[ESP_NOW_ETH_ALEN] = {
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
    };
}

EspNowTransport* EspNowTransport::m_activeInstance = nullptr;

EspNowTransport::~EspNowTransport()
{
    End();
}

bool EspNowTransport::Begin(uint8_t channel, bool wifiPowerSave)
{
    if (m_started)
    {
        return true;
    }
    if (m_activeInstance != nullptr || channel == 0 || channel > 14)
    {
        return false;
    }
    if (!CreateQueues())
    {
        End();
        return false;
    }
    if (!WiFi.mode(WIFI_STA))
    {
        End();
        return false;
    }

    const wifi_ps_type_t powerSaveMode = wifiPowerSave
        ? WIFI_PS_MIN_MODEM
        : WIFI_PS_NONE;
    if (esp_wifi_set_ps(powerSaveMode) != ESP_OK ||
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK)
    {
        End();
        return false;
    }
    if (esp_now_init() != ESP_OK)
    {
        End();
        return false;
    }

    m_espNowInitialized = true;
    m_activeInstance = this;
    if (esp_now_register_recv_cb(OnReceive) != ESP_OK ||
        esp_now_register_send_cb(OnSend) != ESP_OK)
    {
        End();
        return false;
    }

    m_channel = channel;
    if (!RegisterPeer(BroadcastMac))
    {
        End();
        return false;
    }

    m_started = true;
    return true;
}

void EspNowTransport::End()
{
    m_started = false;
    m_sendInFlight = false;

    if (m_espNowInitialized)
    {
        esp_now_unregister_recv_cb();
        esp_now_unregister_send_cb();
    }
    if (m_activeInstance == this)
    {
        m_activeInstance = nullptr;
    }
    if (m_espNowInitialized)
    {
        esp_now_deinit();
        m_espNowInitialized = false;
    }

    m_channel = 0;
    memset(m_inFlightDestinationMac, 0, sizeof(m_inFlightDestinationMac));
    DeleteQueues();
}

bool EspNowTransport::AddPeer(
    const uint8_t destinationMac[ESP_NOW_ETH_ALEN])
{
    if (!m_started)
    {
        return false;
    }
    return RegisterPeer(destinationMac);
}

bool EspNowTransport::RemovePeer(
    const uint8_t destinationMac[ESP_NOW_ETH_ALEN])
{
    if (!m_started || destinationMac == nullptr)
    {
        return false;
    }
    return esp_now_del_peer(destinationMac) == ESP_OK;
}

bool EspNowTransport::HasPeer(
    const uint8_t destinationMac[ESP_NOW_ETH_ALEN]) const
{
    return m_started &&
        destinationMac != nullptr &&
        esp_now_is_peer_exist(destinationMac);
}

bool EspNowTransport::Send(
    const uint8_t destinationMac[ESP_NOW_ETH_ALEN],
    const uint8_t* payload,
    size_t payloadLength)
{
    if (!m_started ||
        destinationMac == nullptr ||
        payload == nullptr ||
        payloadLength == 0 ||
        payloadLength > ESP_NOW_MAX_DATA_LEN)
    {
        return false;
    }

    SendRequest request{};
    memcpy(
        request.destinationMac,
        destinationMac,
        sizeof(request.destinationMac));
    request.payloadLength = static_cast<uint16_t>(payloadLength);
    memcpy(request.payload, payload, payloadLength);
    if (xQueueSend(m_sendQueue, &request, 0) != pdTRUE)
    {
        __atomic_fetch_add(
            &m_sendQueueFullCount,
            1U,
            __ATOMIC_RELAXED);
        return false;
    }

    StartNextSend();
    return true;
}

void EspNowTransport::Update()
{
    if (!m_started)
    {
        return;
    }

    ProcessSendCallbacks();
    StartNextSend();
}

bool EspNowTransport::PeekReceive(EspNowReceivedPacket& packet)
{
    if (m_receivedQueue == nullptr)
    {
        return false;
    }
    return xQueuePeek(m_receivedQueue, &packet, 0) == pdTRUE;
}

bool EspNowTransport::ConsumeReceive()
{
    if (m_receivedQueue == nullptr)
    {
        return false;
    }

    EspNowReceivedPacket discardedPacket{};
    if (xQueueReceive(m_receivedQueue, &discardedPacket, 0) != pdTRUE)
    {
        return false;
    }
    ++m_consumedReceiveCount;
    return true;
}

uint32_t EspNowTransport::GetConsumedReceiveCount() const
{
    return m_consumedReceiveCount;
}

bool EspNowTransport::TryReceive(EspNowReceivedPacket& packet)
{
    if (m_receivedQueue == nullptr)
    {
        return false;
    }
    return xQueueReceive(m_receivedQueue, &packet, 0) == pdTRUE;
}

bool EspNowTransport::TryGetSendResult(EspNowSendResult& result)
{
    Update();
    if (m_sendResultQueue == nullptr)
    {
        return false;
    }
    return xQueueReceive(m_sendResultQueue, &result, 0) == pdTRUE;
}

bool EspNowTransport::IsSendIdle() const
{
    return m_started &&
        !m_sendInFlight &&
        m_sendQueue != nullptr &&
        uxQueueMessagesWaiting(m_sendQueue) == 0;
}

bool EspNowTransport::IsStarted() const
{
    return m_started;
}

EspNowTransportDiagnostics EspNowTransport::GetDiagnostics() const
{
    return EspNowTransportDiagnostics{
        .receivedQueueFullCount = __atomic_load_n(
            &m_receivedQueueFullCount,
            __ATOMIC_RELAXED),
        .sendQueueFullCount = __atomic_load_n(
            &m_sendQueueFullCount,
            __ATOMIC_RELAXED),
        .sendCallbackQueueFullCount = __atomic_load_n(
            &m_sendCallbackQueueFullCount,
            __ATOMIC_RELAXED),
        .sendResultQueueFullCount = __atomic_load_n(
            &m_sendResultQueueFullCount,
            __ATOMIC_RELAXED),
        .sendStartFailureCount = __atomic_load_n(
            &m_sendStartFailureCount,
            __ATOMIC_RELAXED),
    };
}

void EspNowTransport::OnReceive(
    const esp_now_recv_info_t* info,
    const uint8_t* data,
    int dataLength)
{
    EspNowTransport* const instance = m_activeInstance;
    if (instance == nullptr ||
        instance->m_receivedQueue == nullptr ||
        info == nullptr ||
        info->src_addr == nullptr ||
        info->des_addr == nullptr ||
        data == nullptr ||
        dataLength <= 0 ||
        dataLength > ESP_NOW_MAX_DATA_LEN)
    {
        return;
    }

    EspNowReceivedPacket packet{};
    memcpy(packet.sourceMac, info->src_addr, sizeof(packet.sourceMac));
    memcpy(
        packet.destinationMac,
        info->des_addr,
        sizeof(packet.destinationMac));
    packet.receivedTimestampUs = static_cast<uint32_t>(esp_timer_get_time());
    if (info->rx_ctrl != nullptr)
    {
        packet.rssi = info->rx_ctrl->rssi;
        packet.channel = info->rx_ctrl->channel;
        packet.hasRxControl = true;
    }
    packet.payloadLength = static_cast<uint16_t>(dataLength);
    memcpy(packet.payload, data, packet.payloadLength);

    if (xQueueSend(instance->m_receivedQueue, &packet, 0) != pdTRUE)
    {
        __atomic_fetch_add(
            &instance->m_receivedQueueFullCount,
            1U,
            __ATOMIC_RELAXED);
    }
}

void EspNowTransport::OnSend(
    const esp_now_send_info_t* info,
    esp_now_send_status_t status)
{
    EspNowTransport* const instance = m_activeInstance;
    if (instance == nullptr || instance->m_sendCallbackQueue == nullptr)
    {
        return;
    }

    EspNowSendResult result{};
    const uint8_t* destinationMac = instance->m_inFlightDestinationMac;
    if (info != nullptr && info->des_addr != nullptr)
    {
        destinationMac = info->des_addr;
    }
    memcpy(
        result.destinationMac,
        destinationMac,
        sizeof(result.destinationMac));
    result.status = status;
    if (xQueueSend(instance->m_sendCallbackQueue, &result, 0) != pdTRUE)
    {
        __atomic_fetch_add(
            &instance->m_sendCallbackQueueFullCount,
            1U,
            __ATOMIC_RELAXED);
    }
}

bool EspNowTransport::RegisterPeer(
    const uint8_t destinationMac[ESP_NOW_ETH_ALEN])
{
    if (!m_espNowInitialized || destinationMac == nullptr)
    {
        return false;
    }
    if (esp_now_is_peer_exist(destinationMac))
    {
        return true;
    }

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, destinationMac, sizeof(peer.peer_addr));
    peer.ifidx = WIFI_IF_STA;
    peer.channel = m_channel;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

void EspNowTransport::StartNextSend()
{
    while (m_started && !m_sendInFlight)
    {
        SendRequest request{};
        if (xQueuePeek(m_sendQueue, &request, 0) != pdTRUE)
        {
            return;
        }

        memcpy(
            m_inFlightDestinationMac,
            request.destinationMac,
            sizeof(m_inFlightDestinationMac));
        m_sendInFlight = true;
        const esp_err_t sendResult = esp_now_send(
            request.destinationMac,
            request.payload,
            request.payloadLength);
        xQueueReceive(m_sendQueue, &request, 0);
        if (sendResult == ESP_OK)
        {
            return;
        }

        m_sendInFlight = false;
        __atomic_fetch_add(
            &m_sendStartFailureCount,
            1U,
            __ATOMIC_RELAXED);
        EspNowSendResult result{};
        memcpy(
            result.destinationMac,
            request.destinationMac,
            sizeof(result.destinationMac));
        result.status = ESP_NOW_SEND_FAIL;
        if (xQueueSend(m_sendResultQueue, &result, 0) != pdTRUE)
        {
            __atomic_fetch_add(
                &m_sendResultQueueFullCount,
                1U,
                __ATOMIC_RELAXED);
        }
    }
}

void EspNowTransport::ProcessSendCallbacks()
{
    EspNowSendResult result{};
    while (xQueueReceive(m_sendCallbackQueue, &result, 0) == pdTRUE)
    {
        m_sendInFlight = false;
        if (xQueueSend(m_sendResultQueue, &result, 0) != pdTRUE)
        {
            __atomic_fetch_add(
                &m_sendResultQueueFullCount,
                1U,
                __ATOMIC_RELAXED);
        }
    }
}

bool EspNowTransport::CreateQueues()
{
    DeleteQueues();
    m_receivedQueue = xQueueCreate(
        ReceivedQueueLength,
        sizeof(EspNowReceivedPacket));
    m_sendQueue = xQueueCreate(
        SendQueueLength,
        sizeof(SendRequest));
    m_sendCallbackQueue = xQueueCreate(
        SendCallbackQueueLength,
        sizeof(EspNowSendResult));
    m_sendResultQueue = xQueueCreate(
        SendResultQueueLength,
        sizeof(EspNowSendResult));
    return m_receivedQueue != nullptr &&
        m_sendQueue != nullptr &&
        m_sendCallbackQueue != nullptr &&
        m_sendResultQueue != nullptr;
}

void EspNowTransport::DeleteQueues()
{
    if (m_receivedQueue != nullptr)
    {
        vQueueDelete(m_receivedQueue);
        m_receivedQueue = nullptr;
    }
    if (m_sendQueue != nullptr)
    {
        vQueueDelete(m_sendQueue);
        m_sendQueue = nullptr;
    }
    if (m_sendCallbackQueue != nullptr)
    {
        vQueueDelete(m_sendCallbackQueue);
        m_sendCallbackQueue = nullptr;
    }
    if (m_sendResultQueue != nullptr)
    {
        vQueueDelete(m_sendResultQueue);
        m_sendResultQueue = nullptr;
    }
}
