#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <array>
#include <cstdint>
#include <map>

#include "ConfigRuntime.h"
#include "EspNowTransport.h"
#include "NodeStatus.h"

/**
 * @brief 自端末状態のESP-NOWブロードキャスト送受信を管理します。
 */
class EspNowBroadcast
{
public:
    using NodeAddress = std::array<uint8_t, 6>;
    using NodeMap = std::map<NodeAddress, NodeStatus>;
    using NodeLastSeenMap = std::map<NodeAddress, uint32_t>;

    /**
     * @brief 共通transportと実行時設定を使用する管理オブジェクトを生成します。
     *
     * @param transport 共有するESP-NOW transport
     * @param configRuntime 送信する実行時設定
     */
    EspNowBroadcast(
        EspNowTransport& transport,
        ConfigRuntime& configRuntime);

    /**
     * @brief 受信済みNodeStatus通知queueを解放します。
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
     * @brief 受信通知queueを初期化し、初回状態を送信します。
     *
     * @return 初期化できた場合はtrue、それ以外はfalse
     */
    bool Begin();

    /**
     * @brief transport受信queueを処理し、自端末状態を必要な時刻に送信します。
     */
    void Update();

    /**
     * @brief 受信通知queueから最新のノード状態を取得します。
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
     * @brief 指定ノード状態の最終受信時刻を取得します。
     *
     * @param address 対象ノードのMACアドレス
     * @param lastSeenMs 最終受信時刻の格納先
     * @return 最終受信時刻が存在する場合はtrue、それ以外はfalse
     */
    bool GetLastSeenMs(
        const NodeAddress& address,
        uint32_t& lastSeenMs) const;

    /**
     * @brief 現在送信している自端末状態を取得します。
     *
     * @return 自端末のノード状態
     */
    const NodeStatus& GetLocalStatus() const;

    /**
     * @brief 自端末のマスター宣言とセッションIDを更新します。
     * 状態が変化した場合は次回Updateで即時送信します。
     *
     * @param isMaster 自端末がマスターの場合はtrue
     * @param sessionId マスターセッションID
     */
    void SetMasterState(bool isMaster, uint32_t sessionId);

    /**
     * @brief ブロードキャスト管理が開始済みか確認します。
     *
     * @return 開始済みの場合はtrue、それ以外はfalse
     */
    bool IsStarted() const;

private:
    /**
     * @brief transportから取得したNodeStatus packetを検証して保存します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleReceivedPacket(const EspNowReceivedPacket& packet);

    /**
     * @brief 実行時設定を自端末状態へ反映します。
     *
     * @return 送信対象の状態が変化した場合はtrue、それ以外はfalse
     */
    bool RefreshLocalStatus();

    /**
     * @brief 実行時設定から8文字のUWBアドレスを生成します。
     *
     * @param address 生成したアドレスを格納する9バイト領域
     */
    void BuildUwbAddress(char address[9]) const;

    /**
     * @brief 送信期限に達した自端末状態をbroadcast MACへ送信します。
     */
    void SendNodeStatus();

    EspNowTransport& m_transport;
    ConfigRuntime& m_configRuntime;
    QueueHandle_t m_receivedNodeStatusQueue = nullptr;
    NodeStatus m_localStatus{};
    uint32_t m_lastStatusSendMs = 0;
    bool m_sendImmediately = true;
    bool m_started = false;
    NodeMap m_nodes;
    NodeLastSeenMap m_lastSeenByAddress;
};
