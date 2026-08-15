#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief TAG自己位置推定の結果状態を表します。
 */
enum class EnTagPositionEstimateStatus : uint8_t
{
    InsufficientAnchors,
    DegenerateGeometry,
    InvalidInput,
    Available,
};

/**
 * @brief 1台のANCHOR座標とTAGまでの距離を表します。
 */
struct TagPositionAnchorMeasurement
{
    double m_positionXmm = 0.0;
    double m_positionYmm = 0.0;
    double m_distanceMm = 0.0;
    uint8_t m_anchorId = 0U;
};

/**
 * @brief 最小二乗法で求めたTAG自己位置と残差を表します。
 */
struct TagPositionEstimate
{
    EnTagPositionEstimateStatus m_status =
        EnTagPositionEstimateStatus::InsufficientAnchors;
    double m_positionXmm = 0.0;
    double m_positionYmm = 0.0;
    double m_residualRmsMm = 0.0;
    uint8_t m_usedAnchorCount = 0U;
};

/**
 * @brief 可変数ANCHORの測距値から2次元TAG自己位置を推定します。
 */
class TagPositionEstimator
{
public:
    /**
     * @brief 線形化した測距式を最小二乗法で解きます。
     *
     * @param measurements ANCHOR座標と距離の配列
     * @param measurementCount 配列要素数
     * @return 推定位置、残差、結果状態
     */
    static TagPositionEstimate Estimate(
        const TagPositionAnchorMeasurement* measurements,
        size_t measurementCount);
};
