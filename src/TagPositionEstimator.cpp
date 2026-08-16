#include "TagPositionEstimator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr size_t MaximumReportedAnchorCount = 255U;
    constexpr double DeterminantRelativeTolerance = 1.0e-10;

    /**
     * @brief 入力値が有限か確認します。
     *
     * @param measurement 確認するANCHOR測距入力
     * @return 全値が有限で距離が非負の場合はtrue
     */
    bool IsValidMeasurement(
        const TagPositionAnchorMeasurement& measurement)
    {
        return std::isfinite(measurement.m_positionXmm) &&
            std::isfinite(measurement.m_positionYmm) &&
            std::isfinite(measurement.m_distanceMm) &&
            measurement.m_distanceMm >= 0.0;
    }
}

/**
 * @brief 線形化した測距式を最小二乗法で解きます。
 *
 * @param measurements ANCHOR座標と距離の配列
 * @param measurementCount 配列要素数
 * @return 推定位置、残差、結果状態
 */
TagPositionEstimate TagPositionEstimator::Estimate(
    const TagPositionAnchorMeasurement* measurements,
    size_t measurementCount)
{
    TagPositionEstimate estimate{};
    estimate.m_usedAnchorCount = static_cast<uint8_t>(std::min(
        measurementCount,
        MaximumReportedAnchorCount));

    if (measurements == nullptr)
    {
        estimate.m_usedAnchorCount = 0U;
        estimate.m_status = EnTagPositionEstimateStatus::InvalidInput;
        return estimate;
    }
    if (measurementCount < 3U)
    {
        estimate.m_status =
            EnTagPositionEstimateStatus::InsufficientAnchors;
        return estimate;
    }

    for (size_t index = 0; index < measurementCount; ++index)
    {
        if (!IsValidMeasurement(measurements[index]))
        {
            estimate.m_status = EnTagPositionEstimateStatus::InvalidInput;
            return estimate;
        }
    }

    const TagPositionAnchorMeasurement& reference = measurements[0];
    double sumAa = 0.0;
    double sumAb = 0.0;
    double sumBb = 0.0;
    double sumAc = 0.0;
    double sumBc = 0.0;

    for (size_t index = 1U; index < measurementCount; ++index)
    {
        const TagPositionAnchorMeasurement& measurement =
            measurements[index];
        const double a = 2.0 *
            (measurement.m_positionXmm - reference.m_positionXmm);
        const double b = 2.0 *
            (measurement.m_positionYmm - reference.m_positionYmm);
        const double c =
            reference.m_distanceMm * reference.m_distanceMm -
            measurement.m_distanceMm * measurement.m_distanceMm +
            measurement.m_positionXmm * measurement.m_positionXmm -
            reference.m_positionXmm * reference.m_positionXmm +
            measurement.m_positionYmm * measurement.m_positionYmm -
            reference.m_positionYmm * reference.m_positionYmm;

        sumAa += a * a;
        sumAb += a * b;
        sumBb += b * b;
        sumAc += a * c;
        sumBc += b * c;
    }

    const double determinant = sumAa * sumBb - sumAb * sumAb;
    const double determinantScale =
        std::fabs(sumAa * sumBb) + std::fabs(sumAb * sumAb);
    if (!std::isfinite(determinant) ||
        determinantScale <= std::numeric_limits<double>::epsilon() ||
        std::fabs(determinant) <=
            determinantScale * DeterminantRelativeTolerance)
    {
        estimate.m_status =
            EnTagPositionEstimateStatus::DegenerateGeometry;
        return estimate;
    }

    estimate.m_positionXmm =
        (sumAc * sumBb - sumBc * sumAb) / determinant;
    estimate.m_positionYmm =
        (sumAa * sumBc - sumAb * sumAc) / determinant;
    if (!std::isfinite(estimate.m_positionXmm) ||
        !std::isfinite(estimate.m_positionYmm))
    {
        estimate.m_positionXmm = 0.0;
        estimate.m_positionYmm = 0.0;
        estimate.m_status = EnTagPositionEstimateStatus::InvalidInput;
        return estimate;
    }

    double squaredResidualSum = 0.0;
    for (size_t index = 0; index < measurementCount; ++index)
    {
        const double deltaX = estimate.m_positionXmm -
            measurements[index].m_positionXmm;
        const double deltaY = estimate.m_positionYmm -
            measurements[index].m_positionYmm;
        const double predictedDistance = std::hypot(deltaX, deltaY);
        const double residual = predictedDistance -
            measurements[index].m_distanceMm;
        squaredResidualSum += residual * residual;
    }
    estimate.m_residualRmsMm = std::sqrt(
        squaredResidualSum / static_cast<double>(measurementCount));
    estimate.m_status = EnTagPositionEstimateStatus::Available;
    return estimate;
}
