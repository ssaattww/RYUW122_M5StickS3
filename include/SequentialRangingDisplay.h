#pragma once

#include <M5Unified.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "EspNowBroadcast.h"
#include "NtpTimeSynchronizer.h"
#include "RunMode.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"

/**
 * @brief 低優先度の画面タスクへ渡す固定長表示snapshotを表します。
 */
struct SequentialRangingDisplaySnapshot
{
    static constexpr size_t m_maxAnchorResultCount = 5U;
    static constexpr size_t m_visibleNodeCount = 5U;

    TimedRangeMeasurement m_successMeasurements[m_maxAnchorResultCount]{};
    TimedRangeMeasurement m_failureMeasurements[m_maxAnchorResultCount]{};
    NodeStatus m_receivedNodes[m_visibleNodeCount]{};
    EnSequentialRangingState m_state =
        EnSequentialRangingState::WaitingForMaster;
    EnRyuw122InitResult m_ryuw122Result = EnRyuw122InitResult::Ok;
    EnTimeQuality m_timeQuality = EnTimeQuality::Unsynchronized;
    EnRunMode m_mode = EnRunMode::Anchor;
    uint64_t m_currentMasterTimeUs = 0;
    uint8_t m_nodeId = 0;
    size_t m_successMeasurementCount = 0;
    size_t m_failureMeasurementCount = 0;
    size_t m_receivedNodeCount = 0;
    bool m_transportStarted = false;
    bool m_broadcastStarted = false;
    bool m_hasCurrentMasterTime = false;
};

static_assert(
    std::is_trivially_copyable<SequentialRangingDisplaySnapshot>::value,
    "Display snapshot must be queue-copyable");

/**
 * @brief 逐次測距結果、ラウンド統計、受信ノード状態を画面へ表示します。
 */
class SequentialRangingDisplay
{
public:
    /**
     * @brief 逐次測距eventと描画先を注入して表示管理を生成します。
     *
     * @param controller 逐次測距eventの取得元
     * @param broadcast 受信ノード状態の取得元
     * @param timeSynchronizer 現在のマスターTAG基準時刻の取得元
     * @param canvas 描画先Canvas
     */
    SequentialRangingDisplay(
        SequentialRangingController& controller,
        EspNowBroadcast& broadcast,
        NtpTimeSynchronizer& timeSynchronizer,
        M5Canvas& canvas);

    /**
     * @brief 通信とUWBの起動結果を永続表示状態へ保存します。
     *
     * @param ryuw122Result RYUW122の初期化結果
     * @param transportStarted ESP-NOW transportを開始できた場合はtrue
     * @param broadcastStarted NodeStatus broadcastを開始できた場合はtrue
     */
    void SetInitializationHealth(
        EnRyuw122InitResult ryuw122Result,
        bool transportStarted,
        bool broadcastStarted);

    /**
     * @brief 新しい測距結果、ラウンド統計、ノード状態を表示状態へ反映します。
     *
     * @return 再描画が必要な場合はtrue、それ以外はfalse
     */
    bool Update();

    /**
     * @brief 現在の表示modelを固定長snapshotへコピーします。
     *
     * @param snapshot コピー先snapshot
     */
    void CaptureSnapshot(SequentialRangingDisplaySnapshot& snapshot) const;

    /**
     * @brief snapshotから逐次測距状態と受信ノードを描画します。
     *
     * @param snapshot 描画する固定長snapshot
     */
    void Draw(const SequentialRangingDisplaySnapshot& snapshot);

private:
    static constexpr int m_statusBarHeight = 20;
    static constexpr int m_contentLeft = 4;
    static constexpr int m_firstLineY = 23;
    static constexpr int m_lineHeight = 10;
    static constexpr size_t m_visibleNodeCount =
        SequentialRangingDisplaySnapshot::m_visibleNodeCount;
    static constexpr size_t m_maxAnchorResultCount =
        SequentialRangingDisplaySnapshot::m_maxAnchorResultCount;
    static constexpr int m_successHeaderLineIndex = 2;
    static constexpr int m_successFirstLineIndex = 3;
    static constexpr int m_failureHeaderLineIndex = 8;
    static constexpr int m_failureFirstLineIndex = 9;
    static constexpr int m_receivedNodeHeaderLineIndex = 14;
    static constexpr uint64_t m_masterTimeModuloSeconds = 1000000U;

    /**
     * @brief 逐次測距状態の短縮表示名を取得します。
     *
     * @param state 逐次測距状態
     * @return 画面へ表示する状態名
     */
    static const char* GetStateName(EnSequentialRangingState state);

    /**
     * @brief 動作モードと逐次測距状態から役割名を取得します。
     *
     * @param mode 現在の動作モード
     * @param state 現在の逐次測距状態
     * @return マスター、フォロワー、ANCHORを表す短縮名
     */
    static const char* GetRoleName(
        EnRunMode mode,
        EnSequentialRangingState state);

    /**
     * @brief 測距結果状態の短縮表示名を取得します。
     *
     * @param status 測距結果状態
     * @return 画面へ表示する結果名
     */
    static const char* GetFailureName(EnRangeResultStatus status);

    /**
     * @brief 時刻品質の短縮表示名を取得します。
     *
     * @param quality 時刻品質
     * @return 画面へ表示する品質名
     */
    static const char* GetTimeQualityName(EnTimeQuality quality);

    /**
     * @brief 距離を画面幅へ収まる単位へ変換します。
     *
     * @param distanceMm ミリメートル単位の距離
     * @param text 変換後文字列の格納先
     * @param textSize 格納先のバイト数
     */
    static void FormatDistance(
        uint32_t distanceMm,
        char* text,
        size_t textSize);

    /**
     * @brief 初期化失敗が保持されているか確認します。
     *
     * @return RYUW122またはESP-NOWの初期化に失敗した場合はtrue
     */
    static bool HasInitializationFailure(
        const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief 通常表示より優先してsnapshotの初期化失敗を描画します。
     *
     * @param snapshot 描画する固定長snapshot
     */
    void DrawInitializationFailure(
        const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief 自TAGに対するANCHOR別最新測距結果と現在のマスター時刻を描画します。
     *
     * @param snapshot 描画する固定長snapshot
     */
    void DrawTagResults(const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief 自TAG向け測距結果をANCHOR ID昇順の固定長一覧へ保存します。
     *
     * @param measurement 保存候補の測距結果
     * @return 一覧を更新した場合はtrue、それ以外はfalse
     */
    bool StoreTagMeasurement(const TimedRangeMeasurement& measurement);

    /**
     * @brief 成功結果をANCHOR ID昇順のlast-success一覧へ保存します。
     *
     * @param measurement 保存する成功結果
     * @return 一覧を更新した場合はtrue
     */
    bool StoreSuccessMeasurement(const TimedRangeMeasurement& measurement);

    /**
     * @brief 失敗結果をANCHOR ID昇順のcurrent failure一覧へ保存します。
     *
     * @param measurement 保存する失敗結果
     * @return 一覧を更新した場合はtrue
     */
    bool StoreFailureMeasurement(const TimedRangeMeasurement& measurement);

    /**
     * @brief 指定ANCHORのcurrent failureを成功時に解除します。
     *
     * @param anchorId 解除するANCHOR ID
     * @return failureを解除した場合はtrue
     */
    bool RemoveFailureMeasurement(uint8_t anchorId);

    /**
     * @brief 保持中のANCHOR別測距結果と表示品質を破棄します。
     */
    void ClearTagMeasurements();

    /**
     * @brief 現在のマスターTAG基準秒を表示状態へ反映します。
     *
     * @return 表示する秒または有効状態が変化した場合はtrue、それ以外はfalse
     */
    bool UpdateCurrentMasterTime();

    /**
     * @brief snapshotの受信ノード一覧のヘッダーと先頭5件を描画します。
     *
     * @param snapshot 描画する固定長snapshot
     */
    void DrawReceivedNodes(const SequentialRangingDisplaySnapshot& snapshot);

    SequentialRangingController& m_controller;
    EspNowBroadcast& m_broadcast;
    NtpTimeSynchronizer& m_timeSynchronizer;
    M5Canvas& m_canvas;
    TimedRangeMeasurement m_successMeasurements[m_maxAnchorResultCount]{};
    TimedRangeMeasurement m_failureMeasurements[m_maxAnchorResultCount]{};
    EnSequentialRangingState m_latestState =
        EnSequentialRangingState::WaitingForMaster;
    EnRyuw122InitResult m_ryuw122Result = EnRyuw122InitResult::Ok;
    EnTimeQuality m_latestTimeQuality = EnTimeQuality::Unsynchronized;
    uint64_t m_currentMasterTimeUs = 0;
    uint32_t m_latestResetGeneration = 0;
    size_t m_successMeasurementCount = 0;
    size_t m_failureMeasurementCount = 0;
    bool m_transportStarted = false;
    bool m_broadcastStarted = false;
    bool m_hasCurrentMasterTime = false;
};
