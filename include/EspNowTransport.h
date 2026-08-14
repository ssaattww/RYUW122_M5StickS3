#pragma once

#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstddef>
#include <cstdint>

/**
 * @brief ESP-NOWで受信したデータと無線情報の固定長コピーを表します。
 */
struct EspNowReceivedPacket
{
    uint8_t sourceMac[ESP_NOW_ETH_ALEN]{};
    uint8_t destinationMac[ESP_NOW_ETH_ALEN]{};
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t receivedTimestampUs = 0;
    uint16_t payloadLength = 0;
    bool hasRxControl = false;
    uint8_t payload[ESP_NOW_MAX_DATA_LEN]{};
};

/**
 * @brief ESP-NOWの送信完了結果を表します。
 */
struct EspNowSendResult
{
    uint8_t destinationMac[ESP_NOW_ETH_ALEN]{};
    esp_now_send_status_t status = ESP_NOW_SEND_FAIL;
};

/**
 * @brief ESP-NOW transportの診断件数を表します。
 */
struct EspNowTransportDiagnostics
{
    uint32_t receivedQueueFullCount = 0;
    uint32_t sendQueueFullCount = 0;
    uint32_t sendCallbackQueueFullCount = 0;
    uint32_t sendResultQueueFullCount = 0;
    uint32_t sendStartFailureCount = 0;
};

/**
 * @brief raw ESP-NOWの初期化、peer、固定長queueによる送受信を管理します。
 */
class EspNowTransport
{
public:
    /**
     * @brief 未開始状態のESP-NOW transportを生成します。
     */
    EspNowTransport() = default;

    /**
     * @brief ESP-NOW transportを終了して使用中のqueueを解放します。
     */
    ~EspNowTransport();

    /**
     * @brief コピー構築を禁止します。
     *
     * @param other コピー元
     */
    EspNowTransport(const EspNowTransport& other) = delete;

    /**
     * @brief コピー代入を禁止します。
     *
     * @param other コピー元
     * @return 代入先
     */
    EspNowTransport& operator=(const EspNowTransport& other) = delete;

    /**
     * @brief Wi-Fiとraw ESP-NOWを設定値に従って開始します。
     *
     * @param channel 使用するWi-Fiチャンネル
     * @param wifiPowerSave Wi-Fi Modem Sleepを使用する場合はtrue
     * @return 開始できた場合はtrue、それ以外はfalse
     */
    bool Begin(uint8_t channel, bool wifiPowerSave);

    /**
     * @brief raw ESP-NOWを終了してqueueを解放します。
     */
    void End();

    /**
     * @brief ユニキャストまたはブロードキャストpeerを追加します。
     *
     * @param destinationMac 追加するpeerのMACアドレス
     * @return 追加済みまたは追加できた場合はtrue、それ以外はfalse
     */
    bool AddPeer(const uint8_t destinationMac[ESP_NOW_ETH_ALEN]);

    /**
     * @brief 登録済みpeerを削除します。
     *
     * @param destinationMac 削除するpeerのMACアドレス
     * @return 削除できた場合はtrue、それ以外はfalse
     */
    bool RemovePeer(const uint8_t destinationMac[ESP_NOW_ETH_ALEN]);

    /**
     * @brief peerが登録済みか確認します。
     *
     * @param destinationMac 確認するpeerのMACアドレス
     * @return 登録済みの場合はtrue、それ以外はfalse
     */
    bool HasPeer(const uint8_t destinationMac[ESP_NOW_ETH_ALEN]) const;

    /**
     * @brief 固定長FIFOへESP-NOW送信要求を追加します。
     *
     * @param destinationMac 送信先MACアドレス
     * @param payload 送信payload
     * @param payloadLength 送信payloadサイズ
     * @return 送信要求を受理した場合はtrue、それ以外はfalse
     */
    bool Send(
        const uint8_t destinationMac[ESP_NOW_ETH_ALEN],
        const uint8_t* payload,
        size_t payloadLength);

    /**
     * @brief 送信完了を処理し、未送信FIFOの先頭を送信します。
     */
    void Update();

    /**
     * @brief 受信queueの先頭packetを削除せずに取得します。
     * 上位consumerはpacket種別を確認してからConsumeReceive()を呼び出します。
     *
     * @param packet 受信packetの格納先
     * @return packetを取得した場合はtrue、それ以外はfalse
     */
    bool PeekReceive(EspNowReceivedPacket& packet);

    /**
     * @brief 受信queueの先頭packetを1件削除します。
     *
     * @return packetを削除した場合はtrue、それ以外はfalse
     */
    bool ConsumeReceive();

    /**
     * @brief 成功した受信queue削除の累積件数を取得します。
     *
     * @return ConsumeReceive()が成功した累積件数
     */
    uint32_t GetConsumedReceiveCount() const;

    /**
     * @brief 受信queueからpacketを1件取得します。
     *
     * @param packet 受信packetの格納先
     * @return packetを取得した場合はtrue、それ以外はfalse
     */
    bool TryReceive(EspNowReceivedPacket& packet);

    /**
     * @brief 送信完了queueから結果を1件取得します。
     *
     * @param result 送信完了結果の格納先
     * @return 結果を取得した場合はtrue、それ以外はfalse
     */
    bool TryGetSendResult(EspNowSendResult& result);

    /**
     * @brief 送信中packetと送信待ちFIFOがどちらも空か確認します。
     * NTP時刻取得直後の送信に待ち時間を挟まないために使用します。
     *
     * @return transportが開始済みで送信待ちがない場合はtrue
     */
    bool IsSendIdle() const;

    /**
     * @brief ESP-NOWが開始済みか確認します。
     *
     * @return 開始済みの場合はtrue、それ以外はfalse
     */
    bool IsStarted() const;

    /**
     * @brief 現在の診断件数を取得します。
     *
     * @return transportの診断件数
     */
    EspNowTransportDiagnostics GetDiagnostics() const;

private:
    /**
     * @brief 固定長FIFOへ保存するESP-NOW送信要求を表します。
     */
    struct SendRequest
    {
        uint8_t destinationMac[ESP_NOW_ETH_ALEN]{};
        uint16_t payloadLength = 0;
        uint8_t payload[ESP_NOW_MAX_DATA_LEN]{};
    };

    /**
     * @brief ESP-NOW受信callbackで受信情報を固定長queueへコピーします。
     *
     * @param info ESP-NOW受信情報
     * @param data 受信payload
     * @param dataLength 受信payloadサイズ
     */
    static void OnReceive(
        const esp_now_recv_info_t* info,
        const uint8_t* data,
        int dataLength);

    /**
     * @brief ESP-NOW送信callbackで完了情報を固定長queueへコピーします。
     *
     * @param info ESP-NOW送信情報
     * @param status 送信完了状態
     */
    static void OnSend(
        const esp_now_send_info_t* info,
        esp_now_send_status_t status);

    /**
     * @brief ESP-NOWが開始する前にpeerを登録します。
     *
     * @param destinationMac 登録するpeerのMACアドレス
     * @return 登録済みまたは登録できた場合はtrue、それ以外はfalse
     */
    bool RegisterPeer(const uint8_t destinationMac[ESP_NOW_ETH_ALEN]);

    /**
     * @brief 送信中でなければ送信FIFOの先頭を送信します。
     */
    void StartNextSend();

    /**
     * @brief callbackから届いた送信完了を公開queueへ移します。
     */
    void ProcessSendCallbacks();

    /**
     * @brief transportで使用する固定長queueを生成します。
     *
     * @return 全queueを生成できた場合はtrue、それ以外はfalse
     */
    bool CreateQueues();

    /**
     * @brief transportで使用する固定長queueを解放します。
     */
    void DeleteQueues();

    QueueHandle_t m_receivedQueue = nullptr;
    QueueHandle_t m_sendQueue = nullptr;
    QueueHandle_t m_sendCallbackQueue = nullptr;
    QueueHandle_t m_sendResultQueue = nullptr;
    uint8_t m_channel = 0;
    uint8_t m_inFlightDestinationMac[ESP_NOW_ETH_ALEN]{};
    bool m_espNowInitialized = false;
    bool m_sendInFlight = false;
    bool m_started = false;
    uint32_t m_receivedQueueFullCount = 0;
    uint32_t m_consumedReceiveCount = 0;
    uint32_t m_sendQueueFullCount = 0;
    uint32_t m_sendCallbackQueueFullCount = 0;
    uint32_t m_sendResultQueueFullCount = 0;
    uint32_t m_sendStartFailureCount = 0;
    static EspNowTransport* m_activeInstance;
};
