#pragma once

#include <M5Unified.h>

#include <cstddef>
#include <cstdint>

#include "EspNowBroadcast.h"
#include "RunMode.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"

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
     * @param canvas 描画先
     */
    SequentialRangingDisplay(
        SequentialRangingController& controller,
        EspNowBroadcast& broadcast,
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
     * @brief 現在の表示状態を描画します。
     *
     * @param mode 動作モード
     */
    void Draw(EnRunMode mode);

private:
    static constexpr int m_statusBarHeight = 20;
    static constexpr int m_contentLeft = 4;
    static constexpr int m_firstLineY = 23;
    static constexpr int m_lineHeight = 12;
    static constexpr size_t m_visibleNodeCount = 2U;

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
     * @brief 初期化失敗の有無を確認します。
     *
     * @return 初期化失敗がある場合はtrue
     */
    bool HasInitializationFailure() const;

    /**
     * @brief 初期化失敗を描画します。
     */
    void DrawInitializationFailure();

    /**
     * @brief NodeStatus一覧を描画します。
     */
    void DrawReceivedNodes();

    SequentialRangingController& m_controller;
    EspNowBroadcast& m_broadcast;
    M5Canvas& m_canvas;
    TimedRangeMeasurement m_latestMeasurement{};
    SequentialRangeRoundSummary m_latestSummary{};
    EnSequentialRangingState m_latestState =
        EnSequentialRangingState::WaitingForMaster;
    EnRyuw122InitResult m_ryuw122Result = EnRyuw122InitResult::Ok;
    uint32_t m_latestResetGeneration = 0;
    bool m_transportStarted = false;
    bool m_broadcastStarted = false;
    bool m_hasMeasurement = false;
    bool m_hasSummary = false;
};
