#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_wifi.h>

constexpr int ESP_NOW_ETH_ALEN = 6;
constexpr int ESP_NOW_MAX_DATA_LEN = 250;

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;

enum esp_now_send_status_t
{
    ESP_NOW_SEND_SUCCESS = 0,
    ESP_NOW_SEND_FAIL = 1,
};

struct wifi_pkt_rx_ctrl_t
{
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t timestamp = 0;
};

struct esp_now_recv_info_t
{
    const uint8_t* src_addr = nullptr;
    const uint8_t* des_addr = nullptr;
    wifi_pkt_rx_ctrl_t* rx_ctrl = nullptr;
};

struct esp_now_send_info_t
{
    const uint8_t* des_addr = nullptr;
};

struct esp_now_peer_info_t
{
    uint8_t peer_addr[ESP_NOW_ETH_ALEN]{};
    wifi_interface_t ifidx = WIFI_IF_STA;
    uint8_t channel = 0;
    bool encrypt = false;
};

using esp_now_recv_cb_t = void (*)(
    const esp_now_recv_info_t*,
    const uint8_t*,
    int);
using esp_now_send_cb_t = void (*)(
    const esp_now_send_info_t*,
    esp_now_send_status_t);

esp_err_t esp_now_init();
esp_err_t esp_now_deinit();
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback);
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback);
esp_err_t esp_now_unregister_recv_cb();
esp_err_t esp_now_unregister_send_cb();
bool esp_now_is_peer_exist(const uint8_t* peerAddress);
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
esp_err_t esp_now_del_peer(const uint8_t* peerAddress);
esp_err_t esp_now_send(
    const uint8_t* destinationAddress,
    const uint8_t* payload,
    size_t payloadLength);
