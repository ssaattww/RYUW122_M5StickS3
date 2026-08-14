#pragma once

#include <EspNowBus.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstddef>
#include <cstdint>
#include <array>
#include <map>

#include "ConfigRuntime.h"
#include "NodeStatus.h"

/**
 * @brief 自端末状態のESP-NOWブロードキャスト送受信を管理します。
 */
class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;

    /**
     * @brief 実行時設定を使用するブロードキャスト管理オブジェクトを生成します。
     *
     * @param configRuntime 送信する実行時設定
     */
    explicit EspNowBroadcast(ConfigRuntime& configRuntime);

    /**
     * @brief ESP-NOWと受信mailboxを終了します。
     */
    ~EspNowBroadcast();

    /**
     * @brief コピー構築を禁止します。
     *
     * @param other コピー元
     */
    EspNowBroadcast(const EspNowBroadcast& other) = delete;

    /**
     * @brief コピー代入を禁止します。
     *
     * @param other コピー元
     * @return 代入先
     */
    EspNowBroadcast& operator=(const EspNowBroadcast& other) = delete;

    /**
     * @brief 受信mailboxとESP-NOWを初期化し、初回状態を送信します。
     *
     * @return 初期化できた場合はtrue、それ以外はfalse
     */
    bool Begin();

    /**
     * @brief 自端末状態を設定周期でブロードキャスト送信します。
     */
    void Update();

    /**
     * @brief 受信mailboxから最新のノード状態を取得します。
     *
     * @param status 取得したノード状態の格納先
     * @return 受信状態を取得した場合はtrue、それ以外はfalse
     */
    bool TryReceive(NodeStatus& status);

    /**
     * @brief 受信済みノード状態の一覧を取得します。
     *
     * @return MACアドレスをキーとする受信済みノード一覧
     */
    const NodeMap& GetNodes() const;

    /**
     * @brief ESP-NOWが開始済みか確認します。
     *
     * @return 開始済みの場合はtrue、それ以外はfalse
     */
    bool IsStarted() const;

private:
    /**
     * @brief ESP-NOW受信callbackを現在のインスタンスへ転送します。
     *
     * @param mac 送信元MACアドレス
     * @param mac 送信元MACアドレス
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param wasRetry 再送payloadの場合はtrue、それ以外はfalse
     * @param isBroadcast broadcast受信の場合はtrue、それ以外はfalse
     */
    static void OnReceive(
        const uint8_t* mac,
        const uint8_t* data,
        size_t length,
        bool wasRetry,
        bool isBroadcast);

    /**
     * @brief 有効なbroadcastノード状態を受信mailboxへ保存します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param isBroadcast broadcast受信の場合はtrue、それ以外はfalse
     */
    void HandleReceive(
        const uint8_t* mac,
        const uint8_t* data,
        size_t length,
        bool isBroadcast);

    /**
     * @brief 現在の実行時設定をノード状態として送信します。
     */
    void SendNodeStatus();

    ConfigRuntime& m_configRuntime;
    EspNowBus m_espNowBus;
    QueueHandle_t m_receivedNodeStatusQueue = nullptr;
    uint8_t m_macAddress[6]{};
    uint32_t m_lastStatusSendMs = 0;
    bool m_started = false;
    NodeMap m_nodes;
    static EspNowBroadcast* m_activeInstance;
};
