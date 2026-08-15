#ifdef T004_TRANSPORT_TEST

#include "EspNowTestRuntime.h"

#include <WiFi.h>
#include <freertos/queue.h>

#include <cstring>
#include <deque>
#include <set>
#include <vector>

struct TestQueue
{
    UBaseType_t m_capacity = 0;
    UBaseType_t m_itemSize = 0;
    std::deque<std::vector<uint8_t>> m_items;
};

namespace
{
    uint64_t testTimeUs = 0;
    esp_now_recv_cb_t receiveCallback = nullptr;
    esp_now_send_cb_t sendCallback = nullptr;
    std::set<std::vector<uint8_t>> peers;
}

WiFiClass WiFi;

bool WiFiClass::mode(wifi_mode_t mode)
{
    return mode == WIFI_STA;
}

int esp_wifi_set_ps(wifi_ps_type_t mode)
{
    return mode == WIFI_PS_NONE || mode == WIFI_PS_MIN_MODEM
        ? ESP_OK
        : -1;
}

int esp_wifi_set_channel(
    uint8_t channel,
    wifi_second_chan_t secondChannel)
{
    return channel > 0 && channel <= 14 &&
        secondChannel == WIFI_SECOND_CHAN_NONE
        ? ESP_OK
        : -1;
}

esp_err_t esp_now_init()
{
    return ESP_OK;
}

esp_err_t esp_now_deinit()
{
    peers.clear();
    return ESP_OK;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback)
{
    receiveCallback = callback;
    return ESP_OK;
}

esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback)
{
    sendCallback = callback;
    return ESP_OK;
}

esp_err_t esp_now_unregister_recv_cb()
{
    receiveCallback = nullptr;
    return ESP_OK;
}

esp_err_t esp_now_unregister_send_cb()
{
    sendCallback = nullptr;
    return ESP_OK;
}

bool esp_now_is_peer_exist(const uint8_t* peerAddress)
{
    if (peerAddress == nullptr)
    {
        return false;
    }
    return peers.count(std::vector<uint8_t>(
        peerAddress,
        peerAddress + ESP_NOW_ETH_ALEN)) != 0;
}

esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer)
{
    if (peer == nullptr)
    {
        return -1;
    }
    peers.insert(std::vector<uint8_t>(
        peer->peer_addr,
        peer->peer_addr + ESP_NOW_ETH_ALEN));
    return ESP_OK;
}

esp_err_t esp_now_del_peer(const uint8_t* peerAddress)
{
    if (peerAddress == nullptr)
    {
        return -1;
    }
    peers.erase(std::vector<uint8_t>(
        peerAddress,
        peerAddress + ESP_NOW_ETH_ALEN));
    return ESP_OK;
}

esp_err_t esp_now_send(
    const uint8_t* destinationAddress,
    const uint8_t* payload,
    size_t payloadLength)
{
    return destinationAddress != nullptr &&
        payload != nullptr &&
        payloadLength > 0
        ? ESP_OK
        : -1;
}

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize)
{
    TestQueue* queue = new TestQueue{};
    queue->m_capacity = length;
    queue->m_itemSize = itemSize;
    return queue;
}

void vQueueDelete(QueueHandle_t queue)
{
    delete queue;
}

BaseType_t xQueueSend(
    QueueHandle_t queue,
    const void* item,
    uint32_t waitTicks)
{
    (void)waitTicks;
    if (queue == nullptr ||
        item == nullptr ||
        queue->m_items.size() >= queue->m_capacity)
    {
        return pdFALSE;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(item);
    queue->m_items.emplace_back(bytes, bytes + queue->m_itemSize);
    return pdTRUE;
}

BaseType_t xQueuePeek(
    QueueHandle_t queue,
    void* item,
    uint32_t waitTicks)
{
    (void)waitTicks;
    if (queue == nullptr || item == nullptr || queue->m_items.empty())
    {
        return pdFALSE;
    }
    memcpy(item, queue->m_items.front().data(), queue->m_itemSize);
    return pdTRUE;
}

BaseType_t xQueueReceive(
    QueueHandle_t queue,
    void* item,
    uint32_t waitTicks)
{
    if (xQueuePeek(queue, item, waitTicks) != pdTRUE)
    {
        return pdFALSE;
    }
    queue->m_items.pop_front();
    return pdTRUE;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue)
{
    return queue == nullptr
        ? 0U
        : static_cast<UBaseType_t>(queue->m_items.size());
}

extern "C" int64_t esp_timer_get_time()
{
    return static_cast<int64_t>(testTimeUs);
}

void SetEspNowTestTimeUs(uint64_t timeUs)
{
    testTimeUs = timeUs;
}

bool InvokeEspNowTestReceive(
    const esp_now_recv_info_t& info,
    const uint8_t* payload,
    int payloadLength)
{
    if (receiveCallback == nullptr)
    {
        return false;
    }
    receiveCallback(&info, payload, payloadLength);
    return true;
}

#endif
