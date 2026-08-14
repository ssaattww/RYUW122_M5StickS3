#pragma once

#include <M5Unified.h>

#include <cstddef>
#include <cstdint>

#include "EspNowBroadcast.h"
#include "RunMode.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"

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
     * @param canvas 描画先Canvas
     */
    SequentialRangingDisplay(
        SequentialRangingController& controller,
        EspNowBroadcast& broadcast,
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
     * @brief ステータスバーを避けて逐次測距状態と受信ノードを描画します。
     *
     * @param mode 現在の動作モード
     */
    void Draw(EnRunMode mode);

private:
    static constexpr int m_statusBarHeight = 20;
    static constexpr int m_contentLeft = 4;
    static constexpr int m_firstLineY = 23;
    static constexpr int m_lineHeight = 12;
    static constexpr size_t m_visibleNodeCount = 2U;

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
    static const char* GetResultName(EnRangeResultStatus status);

    /**
     * @brief 時刻品質の短縮表示名を取得します。
     *
     * @param quality 時刻品質
     * @return 画面へ表示する品質名
     */
    static const char* GetTimeQualityName(EnTimeQuality quality);

    /**
     * @brief 初期化失敗が保持されているか確認します。
     *
     * @return RYUW122またはESP-NOWの初期化に失敗した場合はtrue
     */
    bool HasInitializationFailure() const;

    /**
     * @brief 通常表示より優先して保持中の初期化失敗を描画します。
     */
    void DrawInitializationFailure();

    /**
     * @brief 受信ノード一覧のヘッダーと先頭2件を描画します。
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
