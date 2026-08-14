#include "EspNowBroadcast.h"

#include <Arduino.h>
#include <esp_wifi.h>

#include <cstring>

namespace
{
    constexpr UBaseType_t ReceivedNodeStatusQueueLength = 16;
    constexpr uint32_t StatusSendIntervalMs = 10000;
}

EspNowBroadcast* EspNowBroadcast::m_activeInstance = nullptr;

/**
 * @brief 実行時設定を使用するブロードキャスト管理オブジェクトを生成します。
 *
 * @param configRuntime 送信する実行時設定
 */
EspNowBroadcast::EspNowBroadcast(ConfigRuntime& configRuntime)
    : m_configRuntime(configRuntime)
{
}

/**
 * @brief ESP-NOWと受信mailboxを終了します。
 */
EspNowBroadcast::~EspNowBroadcast()
{
    if (m_activeInstance == this)
    {
        m_activeInstance = nullptr;
    }

    if (m_started)
    {
        m_espNowBus.end();
        m_started = false;
    }

    if (m_receivedNodeStatusQueue != nullptr)
    {
        vQueueDelete(m_receivedNodeStatusQueue);
        m_receivedNodeStatusQueue = nullptr;
    }
}

/**
 * @brief 受信mailboxとESP-NOWを初期化し、初回状態を送信します。
 *
 * @return 初期化できた場合はtrue、それ以外はfalse
 */
bool EspNowBroadcast::Begin()
{
    if (m_started)
    {
        return true;
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

    EspNowBus::Config config;
    config.groupName = "RYUW122";
    config.channel = static_cast<int8_t>(
        m_configRuntime.GetCurrentEspnowChannel());
    config.autoJoinIntervalMs = 10000;

    m_activeInstance = this;
    m_espNowBus.onReceive(OnReceive);
    if (!m_espNowBus.begin(config))
    {
        m_activeInstance = nullptr;
        return false;
    }

    if (esp_wifi_get_mac(WIFI_IF_STA, m_macAddress) != ESP_OK)
    {
        m_activeInstance = nullptr;
        m_espNowBus.end();
        return false;
    }

    m_started = true;
    m_lastStatusSendMs = millis() - StatusSendIntervalMs;
    SendNodeStatus();
    return true;
}

/**
 * @brief 自端末状態を設定周期でブロードキャスト送信します。
 */
void EspNowBroadcast::Update()
{
    SendNodeStatus();
}

/**
 * @brief 受信mailboxから最新のノード状態を取得します。
 *
 * @param status 取得したノード状態の格納先
 * @return 受信状態を取得した場合はtrue、それ以外はfalse
 */
bool EspNowBroadcast::TryReceive(NodeStatus& status)
{
    if (m_receivedNodeStatusQueue == nullptr)
    {
        return false;
    }

    NodeStatus receivedStatus{};
    if (xQueueReceive(
            m_receivedNodeStatusQueue,
            &receivedStatus,
            0) != pdTRUE)
    {
        return false;
    }

    const NodeAddress address{
        receivedStatus.macAddress[0],
        receivedStatus.macAddress[1],
        receivedStatus.macAddress[2],
        receivedStatus.macAddress[3],
        receivedStatus.macAddress[4],
        receivedStatus.macAddress[5],
    };
    m_nodes[address] = receivedStatus;
    status = receivedStatus;
    return true;
}

/**
 * @brief 受信済みノード状態の一覧を取得します。
 *
 * @return MACアドレスをキーとする受信済みノード一覧
 */
const EspNowBroadcast::NodeMap& EspNowBroadcast::GetNodes() const
{
    return m_nodes;
}

/**
 * @brief ESP-NOWが開始済みか確認します。
 *
 * @return 開始済みの場合はtrue、それ以外はfalse
 */
bool EspNowBroadcast::IsStarted() const
{
    return m_started;
}

/**
 * @brief ESP-NOW受信callbackを現在のインスタンスへ転送します。
 *
 * @param mac 送信元MACアドレス
 * @param data 受信payload
 * @param length 受信payloadサイズ
 * @param wasRetry 再送payloadの場合はtrue、それ以外はfalse
 * @param isBroadcast broadcast受信の場合はtrue、それ以外はfalse
 */
void EspNowBroadcast::OnReceive(
    const uint8_t* mac,
    const uint8_t* data,
    size_t length,
    bool wasRetry,
    bool isBroadcast)
{
    static_cast<void>(mac);
    static_cast<void>(wasRetry);

    if (m_activeInstance != nullptr)
    {
        m_activeInstance->HandleReceive(
            mac,
            data,
            length,
            isBroadcast);
    }
}

/**
 * @brief 有効なbroadcastノード状態を受信mailboxへ保存します。
 *
 * @param mac 送信元MACアドレス
 * @param data 受信payload
 * @param length 受信payloadサイズ
 * @param isBroadcast broadcast受信の場合はtrue、それ以外はfalse
 */
void EspNowBroadcast::HandleReceive(
    const uint8_t* mac,
    const uint8_t* data,
    size_t length,
    bool isBroadcast)
{
    if (!isBroadcast ||
        mac == nullptr ||
        data == nullptr ||
        length != sizeof(NodeStatus) ||
        m_receivedNodeStatusQueue == nullptr)
    {
        return;
    }

    NodeStatus status{};
    memcpy(&status, data, sizeof(status));
    if (status.mode != EnRunMode::Tag &&
        status.mode != EnRunMode::Anchor)
    {
        return;
    }

    memcpy(status.macAddress, mac, sizeof(status.macAddress));
    xQueueSend(m_receivedNodeStatusQueue, &status, 0);
}

/**
 * @brief 現在の実行時設定をノード状態として送信します。
 */
void EspNowBroadcast::SendNodeStatus()
{
    if (!m_started)
    {
        return;
    }

    const uint32_t now = millis();
    if (now - m_lastStatusSendMs < StatusSendIntervalMs)
    {
        return;
    }

    const NodeStatus nodeStatus{
        .anchorPositionX = m_configRuntime.GetAnchorPositionX(),
        .anchorPositionY = m_configRuntime.GetAnchorPositionY(),
        .macAddress = {
            m_macAddress[0],
            m_macAddress[1],
            m_macAddress[2],
            m_macAddress[3],
            m_macAddress[4],
            m_macAddress[5],
        },
        .nodeID = m_configRuntime.GetCurrentNodeID(),
        .mode = m_configRuntime.GetRunMode(),
    };

    m_espNowBus.broadcast(
        &nodeStatus,
        sizeof(nodeStatus),
        0);
    m_lastStatusSendMs = now;
}
