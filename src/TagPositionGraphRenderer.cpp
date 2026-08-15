#include "TagPositionGraphRenderer.h"

#include <algorithm>
#include <cmath>

#include "NodeStatus.h"

/**
 * @brief snapshotが位置グラフ表示可能な状態か確認します。
 *
 * @param snapshot 確認する表示snapshot
 * @return TAGかつ初期化正常の場合はtrue
 */
bool TagPositionGraphRenderer::CanDraw(
    const SequentialRangingDisplaySnapshot& snapshot)
{
    return snapshot.m_mode == EnRunMode::Tag &&
        snapshot.m_ryuw122Result == EnRyuw122InitResult::Ok &&
        snapshot.m_transportStarted &&
        snapshot.m_broadcastStarted;
}

/**
 * @brief snapshot内の最新測距値とANCHOR座標から位置グラフを描画します。
 *
 * @param snapshot 描画元snapshot
 * @param canvas 描画先Canvas
 */
void TagPositionGraphRenderer::Draw(
    const SequentialRangingDisplaySnapshot& snapshot,
    M5Canvas& canvas)
{
    canvas.fillRect(
        0,
        20,
        canvas.width(),
        canvas.height() - 20,
        TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(1);
    canvas.setCursor(m_contentLeft, m_firstLineY);
    canvas.print("TAG POSITION A:LIST");

    TagPositionAnchorMeasurement measurements[
        SequentialRangingDisplaySnapshot::m_maxAnchorResultCount]{};
    const size_t measurementCount = BuildMeasurements(
        snapshot,
        measurements,
        sizeof(measurements) / sizeof(*measurements));
    const TagPositionEstimate estimate = TagPositionEstimator::Estimate(
        measurements,
        measurementCount);

    DrawEstimateText(estimate, canvas);
    DrawGraph(measurements, measurementCount, estimate, canvas);
}

/**
 * @brief 成功測距と受信NodeStatusをANCHOR IDで結合します。
 *
 * @param snapshot 結合元snapshot
 * @param measurements 結合結果格納先
 * @param capacity 格納先要素数
 * @return 結合できたANCHOR数
 */
size_t TagPositionGraphRenderer::BuildMeasurements(
    const SequentialRangingDisplaySnapshot& snapshot,
    TagPositionAnchorMeasurement* measurements,
    size_t capacity)
{
    if (measurements == nullptr || capacity == 0U)
    {
        return 0U;
    }

    size_t count = 0U;
    for (size_t measurementIndex = 0U;
         measurementIndex < snapshot.m_successMeasurementCount &&
             count < capacity;
         ++measurementIndex)
    {
        const TimedRangeMeasurement& range =
            snapshot.m_successMeasurements[measurementIndex];
        const NodeStatus* matchedAnchor = nullptr;
        size_t matchCount = 0U;
        for (size_t nodeIndex = 0U;
             nodeIndex < snapshot.m_receivedNodeCount;
             ++nodeIndex)
        {
            const NodeStatus& node = snapshot.m_receivedNodes[nodeIndex];
            if (node.mode == EnRunMode::Anchor &&
                node.nodeID == range.anchorId)
            {
                matchedAnchor = &node;
                ++matchCount;
            }
        }
        if (matchCount != 1U || matchedAnchor == nullptr)
        {
            continue;
        }

        measurements[count].m_anchorId = range.anchorId;
        measurements[count].m_positionXmm =
            static_cast<double>(matchedAnchor->anchorPositionX);
        measurements[count].m_positionYmm =
            static_cast<double>(matchedAnchor->anchorPositionY);
        measurements[count].m_distanceMm =
            static_cast<double>(range.distanceMm);
        ++count;
    }
    return count;
}

/**
 * @brief 推定状態と数値をグラフ上部へ描画します。
 *
 * @param estimate 位置推定結果
 * @param canvas 描画先Canvas
 */
void TagPositionGraphRenderer::DrawEstimateText(
    const TagPositionEstimate& estimate,
    M5Canvas& canvas)
{
    canvas.setCursor(m_contentLeft, m_firstLineY + m_lineHeight);
    switch (estimate.m_status)
    {
        case EnTagPositionEstimateStatus::Available:
            canvas.printf(
                "X:%ldmm",
                static_cast<long>(std::llround(estimate.m_positionXmm)));
            canvas.setCursor(
                m_contentLeft,
                m_firstLineY + m_lineHeight * 2);
            canvas.printf(
                "Y:%ldmm",
                static_cast<long>(std::llround(estimate.m_positionYmm)));
            canvas.setCursor(
                m_contentLeft,
                m_firstLineY + m_lineHeight * 3);
            canvas.printf(
                "A:%u RMS:%lumm",
                estimate.m_usedAnchorCount,
                static_cast<unsigned long>(std::llround(
                    estimate.m_residualRmsMm)));
            break;
        case EnTagPositionEstimateStatus::InsufficientAnchors:
            canvas.print("NEED 3 ANCHORS");
            canvas.setCursor(
                m_contentLeft,
                m_firstLineY + m_lineHeight * 2);
            canvas.printf("FOUND:%u", estimate.m_usedAnchorCount);
            break;
        case EnTagPositionEstimateStatus::DegenerateGeometry:
            canvas.print("GEOMETRY ERROR");
            canvas.setCursor(
                m_contentLeft,
                m_firstLineY + m_lineHeight * 2);
            canvas.printf("ANCHORS:%u", estimate.m_usedAnchorCount);
            break;
        case EnTagPositionEstimateStatus::InvalidInput:
        default:
            canvas.print("POSITION INVALID");
            break;
    }
}

/**
 * @brief ANCHORと有効なTAG位置を自動スケールで描画します。
 *
 * @param measurements 描画するANCHOR配列
 * @param measurementCount ANCHOR数
 * @param estimate 位置推定結果
 * @param canvas 描画先Canvas
 */
void TagPositionGraphRenderer::DrawGraph(
    const TagPositionAnchorMeasurement* measurements,
    size_t measurementCount,
    const TagPositionEstimate& estimate,
    M5Canvas& canvas)
{
    const int left = m_contentLeft + m_graphMargin;
    const int right = canvas.width() - m_graphMargin - 1;
    const int top = m_graphTop + m_graphMargin;
    const int bottom = canvas.height() - m_graphMargin - 1;
    if (right <= left || bottom <= top)
    {
        return;
    }

    canvas.fillRect(left, top, right - left + 1, 1, TFT_WHITE);
    canvas.fillRect(left, bottom, right - left + 1, 1, TFT_WHITE);
    canvas.fillRect(left, top, 1, bottom - top + 1, TFT_WHITE);
    canvas.fillRect(right, top, 1, bottom - top + 1, TFT_WHITE);
    if (measurements == nullptr || measurementCount == 0U)
    {
        return;
    }

    double minimumX = measurements[0].m_positionXmm;
    double maximumX = minimumX;
    double minimumY = measurements[0].m_positionYmm;
    double maximumY = minimumY;
    for (size_t index = 1U; index < measurementCount; ++index)
    {
        minimumX = std::min(minimumX, measurements[index].m_positionXmm);
        maximumX = std::max(maximumX, measurements[index].m_positionXmm);
        minimumY = std::min(minimumY, measurements[index].m_positionYmm);
        maximumY = std::max(maximumY, measurements[index].m_positionYmm);
    }
    if (estimate.m_status == EnTagPositionEstimateStatus::Available)
    {
        minimumX = std::min(minimumX, estimate.m_positionXmm);
        maximumX = std::max(maximumX, estimate.m_positionXmm);
        minimumY = std::min(minimumY, estimate.m_positionYmm);
        maximumY = std::max(maximumY, estimate.m_positionYmm);
    }

    const double rangeX = maximumX - minimumX;
    const double rangeY = maximumY - minimumY;
    const double paddingX = std::max(100.0, rangeX * 0.05);
    const double paddingY = std::max(100.0, rangeY * 0.05);
    minimumX -= paddingX;
    maximumX += paddingX;
    minimumY -= paddingY;
    maximumY += paddingY;

    for (size_t index = 0U; index < measurementCount; ++index)
    {
        const int x = MapX(
            measurements[index].m_positionXmm,
            minimumX,
            maximumX,
            left + 2,
            right - 2);
        const int y = MapY(
            measurements[index].m_positionYmm,
            minimumY,
            maximumY,
            top + 2,
            bottom - 2);
        canvas.fillRect(x - 1, y - 1, 3, 3, TFT_WHITE);
    }

    if (estimate.m_status == EnTagPositionEstimateStatus::Available)
    {
        const int x = MapX(
            estimate.m_positionXmm,
            minimumX,
            maximumX,
            left + 2,
            right - 2);
        const int y = MapY(
            estimate.m_positionYmm,
            minimumY,
            maximumY,
            top + 2,
            bottom - 2);
        canvas.fillRect(x - 2, y - 2, 5, 5, TFT_RED);
    }
}

/**
 * @brief 実座標をグラフX座標へ変換します。
 */
int TagPositionGraphRenderer::MapX(
    double value,
    double minimum,
    double maximum,
    int left,
    int right)
{
    if (maximum <= minimum || right <= left)
    {
        return left;
    }
    const double unboundedRatio =
        (value - minimum) / (maximum - minimum);
    const double ratio = std::max(0.0, std::min(1.0, unboundedRatio));
    return left + static_cast<int>(std::lround(
        ratio * static_cast<double>(right - left)));
}

/**
 * @brief 実座標を上向き正のグラフY座標へ変換します。
 */
int TagPositionGraphRenderer::MapY(
    double value,
    double minimum,
    double maximum,
    int top,
    int bottom)
{
    if (maximum <= minimum || bottom <= top)
    {
        return bottom;
    }
    const double unboundedRatio =
        (value - minimum) / (maximum - minimum);
    const double ratio = std::max(0.0, std::min(1.0, unboundedRatio));
    return bottom - static_cast<int>(std::lround(
        ratio * static_cast<double>(bottom - top)));
}
