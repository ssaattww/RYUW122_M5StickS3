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
 * @brief test用の固定長表示snapshotを表します。
 */
struct SequentialRangingDisplaySnapshot
{
    static constexpr size_t m_maxAnchorResultCount = 8U;
    static constexpr size_t m_visibleNodeCount = 3U;

    TimedRangeMeasurement m_anchorMeasurements[m_maxAnchorResultCount]{};
    NodeStatus m_receivedNodes[m_visibleNodeCount]{};
    EnSequentialRangingState m_state =
        EnSequentialRangingState::WaitingForMaster;
    EnRyuw122InitResult m_ryuw122Result = EnRyuw122InitResult::Ok;
    EnTimeQuality m_timeQuality = EnTimeQuality::Unsynchronized;
    EnRunMode m_mode = EnRunMode::Anchor;
    uint64_t m_currentMasterTimeUs = 0;
    uint8_t m_nodeId = 0;
    size_t m_anchorMeasurementCount = 0;
    size_t m_receivedNodeCount = 0;
    bool m_transportStarted = false;
    bool m_broadcastStarted = false;
    bool m_hasCurrentMasterTime = false;
};

static_assert(
    std::is_trivially_copyable<SequentialRangingDisplaySnapshot>::value,
    "Display snapshot must be queue-copyable");

/**
 * @brief test対象の逐次測距画面表示を表します。
 */
class SequentialRangingDisplay
{
public:
    /**
     * @brief 依存を注入して表示管理を生成します。
     *
     * @param controller 逐次測距event取得元
     * @param broadcast NodeStatus取得元
     * @param timeSynchronizer 現在のマスターTAG基準時刻取得元
     * @param canvas 描画先
     */
    SequentialRangingDisplay(
        SequentialRangingController& controller,
        EspNowBroadcast& broadcast,
        NtpTimeSynchronizer& timeSynchronizer,
        M5Canvas& canvas);

    /**
     * @brief 初期化healthを保存します。
     *
     * @param ryuw122Result RYUW122初期化結果
     * @param transportStarted transport開始結果
     * @param broadcastStarted broadcast開始結果
     */
    void SetInitializationHealth(
        EnRyuw122InitResult ryuw122Result,
        bool transportStarted,
        bool broadcastStarted);

    /**
     * @brief eventを表示状態へ反映します。
     *
     * @return 再描画が必要な場合はtrue
     */
    bool Update();

    /**
     * @brief 現在の表示modelをsnapshotへコピーします。
     *
     * @param snapshot コピー先snapshot
     */
    void CaptureSnapshot(SequentialRangingDisplaySnapshot& snapshot) const;

    /**
     * @brief snapshotを描画します。
     *
     * @param snapshot 描画対象snapshot
     */
    void Draw(const SequentialRangingDisplaySnapshot& snapshot);

private:
    static constexpr int m_statusBarHeight = 20;
    static constexpr int m_contentLeft = 4;
    static constexpr int m_firstLineY = 23;
    static constexpr int m_lineHeight = 12;
    static constexpr size_t m_visibleNodeCount =
        SequentialRangingDisplaySnapshot::m_visibleNodeCount;
    static constexpr size_t m_maxAnchorResultCount =
        SequentialRangingDisplaySnapshot::m_maxAnchorResultCount;
    static constexpr int m_tagResultFirstLineIndex = 2;
    static constexpr int m_receivedNodeHeaderLineIndex = 10;
    static constexpr float m_tagResultTextScaleX = 1.0F;
    static constexpr uint64_t m_masterTimeModuloSeconds = 1000000U;

    /**
     * @brief 状態の短縮表示名を取得します。
     *
     * @param state 逐次測距状態
     * @return 状態の短縮表示名
     */
    static const char* GetStateName(EnSequentialRangingState state);

    /**
     * @brief 役割の短縮表示名を取得します。
     *
     * @param mode 動作モード
     * @param state 逐次測距状態
     * @return 役割の短縮表示名
     */
    static const char* GetRoleName(
        EnRunMode mode,
        EnSequentialRangingState state);

    /**
     * @brief 結果の短縮表示名を取得します。
     *
     * @param status 測距結果状態
     * @return 結果の短縮表示名
     */
    static const char* GetResultName(EnRangeResultStatus status);

    /**
     * @brief 時刻品質の短縮表示名を取得します。
     *
     * @param quality 時刻品質
     * @return 時刻品質の短縮表示名
     */
    static const char* GetTimeQualityName(EnTimeQuality quality);

    /**
     * @brief 距離を画面幅へ収まる単位へ変換します。
     *
     * @param distanceMm ミリメートル単位の距離
     * @param text 変換後文字列格納先
     * @param textSize 格納先バイト数
     */
    static void FormatDistance(
        uint32_t distanceMm,
        char* text,
        size_t textSize);

    /**
     * @brief 測距結果が有効なマスターTAG基準計測時刻を持つか確認します。
     *
     * @param measurement 確認する測距結果
     * @return 時刻変換済みの品質である場合はtrue
     */
    static bool HasValidMeasurementMasterTime(
        const TimedRangeMeasurement& measurement);

    /**
     * @brief 初期化失敗の有無を確認します。
     *
     * @return 初期化失敗がある場合はtrue
     */
    static bool HasInitializationFailure(
        const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief 初期化失敗を描画します。
     */
    void DrawInitializationFailure(
        const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief 自TAGのANCHOR別結果と現在マスター時刻を描画します。
     */
    void DrawTagResults(const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief 自TAG向け結果をANCHOR ID昇順の固定長一覧へ保存します。
     *
     * @param measurement 保存候補の測距結果
     * @return 一覧を更新した場合はtrue
     */
    bool StoreTagMeasurement(const TimedRangeMeasurement& measurement);

    /**
     * @brief 保持中のANCHOR別結果を破棄します。
     */
    void ClearTagMeasurements();

    /**
     * @brief 現在のマスターTAG基準秒を表示状態へ反映します。
     *
     * @return 表示状態が変化した場合はtrue
     */
    bool UpdateCurrentMasterTime();

    /**
     * @brief NodeStatus一覧の先頭3件を描画します。
     */
    void DrawReceivedNodes(const SequentialRangingDisplaySnapshot& snapshot);

    SequentialRangingController& m_controller;
    EspNowBroadcast& m_broadcast;
    NtpTimeSynchronizer& m_timeSynchronizer;
    M5Canvas& m_canvas;
    TimedRangeMeasurement m_anchorMeasurements[m_maxAnchorResultCount]{};
    EnSequentialRangingState m_latestState =
        EnSequentialRangingState::WaitingForMaster;
    EnRyuw122InitResult m_ryuw122Result = EnRyuw122InitResult::Ok;
    EnTimeQuality m_latestTimeQuality = EnTimeQuality::Unsynchronized;
    uint64_t m_currentMasterTimeUs = 0;
    uint32_t m_latestResetGeneration = 0;
    size_t m_anchorMeasurementCount = 0;
    bool m_transportStarted = false;
    bool m_broadcastStarted = false;
    bool m_hasCurrentMasterTime = false;
};
