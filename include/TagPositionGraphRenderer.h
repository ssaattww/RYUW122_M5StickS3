#pragma once

#include <M5Unified.h>

#include "SequentialRangingDisplay.h"
#include "TagPositionEstimator.h"

/**
 * @brief TAG自己位置とANCHOR配置をM5画面へ描画します。
 */
class TagPositionGraphRenderer
{
public:
    /**
     * @brief snapshotが位置グラフ表示可能な状態か確認します。
     *
     * @param snapshot 確認する表示snapshot
     * @return TAGかつ初期化正常の場合はtrue
     */
    static bool CanDraw(
        const SequentialRangingDisplaySnapshot& snapshot);

    /**
     * @brief snapshot内の最新測距値とANCHOR座標から位置グラフを描画します。
     *
     * @param snapshot 描画元snapshot
     * @param canvas 描画先Canvas
     */
    static void Draw(
        const SequentialRangingDisplaySnapshot& snapshot,
        M5Canvas& canvas);

private:
    static constexpr int m_contentLeft = 4;
    static constexpr int m_firstLineY = 23;
    static constexpr int m_lineHeight = 10;
    static constexpr int m_graphTop = 66;
    static constexpr int m_graphMargin = 5;

    /**
     * @brief 成功測距と受信NodeStatusをANCHOR IDで結合します。
     *
     * @param snapshot 結合元snapshot
     * @param measurements 結合結果格納先
     * @param capacity 格納先要素数
     * @return 結合できたANCHOR数
     */
    static size_t BuildMeasurements(
        const SequentialRangingDisplaySnapshot& snapshot,
        TagPositionAnchorMeasurement* measurements,
        size_t capacity);

    /**
     * @brief 推定状態と数値をグラフ上部へ描画します。
     *
     * @param estimate 位置推定結果
     * @param canvas 描画先Canvas
     */
    static void DrawEstimateText(
        const TagPositionEstimate& estimate,
        M5Canvas& canvas);

    /**
     * @brief ANCHORと有効なTAG位置を自動スケールで描画します。
     *
     * @param measurements 描画するANCHOR配列
     * @param measurementCount ANCHOR数
     * @param estimate 位置推定結果
     * @param canvas 描画先Canvas
     */
    static void DrawGraph(
        const TagPositionAnchorMeasurement* measurements,
        size_t measurementCount,
        const TagPositionEstimate& estimate,
        M5Canvas& canvas);

    /**
     * @brief 実座標をグラフX座標へ変換します。
     */
    static int MapX(
        double value,
        double minimum,
        double maximum,
        int left,
        int right);

    /**
     * @brief 実座標を上向き正のグラフY座標へ変換します。
     */
    static int MapY(
        double value,
        double minimum,
        double maximum,
        int top,
        int bottom);
};
