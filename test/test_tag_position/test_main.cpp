#include <unity.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "RunMode.h"
#include "TagPositionEstimator.h"
#include "TagPositionViewController.h"

namespace
{
    /**
     * @brief test用ANCHOR測距入力を生成します。
     *
     * @param anchorId ANCHOR ID
     * @param positionXmm ANCHOR X座標[mm]
     * @param positionYmm ANCHOR Y座標[mm]
     * @param distanceMm TAGまでの距離[mm]
     * @return test用ANCHOR測距入力
     */
    TagPositionAnchorMeasurement MakeAnchor(
        uint8_t anchorId,
        double positionXmm,
        double positionYmm,
        double distanceMm)
    {
        TagPositionAnchorMeasurement measurement{};
        measurement.m_anchorId = anchorId;
        measurement.m_positionXmm = positionXmm;
        measurement.m_positionYmm = positionYmm;
        measurement.m_distanceMm = distanceMm;
        return measurement;
    }
}

/**
 * @brief 各test前の初期化を行います。
 */
void setUp()
{
}

/**
 * @brief 各test後の後処理を行います。
 */
void tearDown()
{
}

/**
 * @brief 3 ANCHORの厳密解からTAG座標を求められることを確認します。
 */
void TestThreeAnchorsEstimateExactPosition()
{
    const TagPositionAnchorMeasurement anchors[] = {
        MakeAnchor(1U, 0.0, 0.0, 5000.0),
        MakeAnchor(2U, 6000.0, 0.0, 5000.0),
        MakeAnchor(3U, 0.0, 8000.0, 5000.0),
    };

    const TagPositionEstimate estimate = TagPositionEstimator::Estimate(
        anchors,
        sizeof(anchors) / sizeof(*anchors));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionEstimateStatus::Available),
        static_cast<int>(estimate.m_status));
    TEST_ASSERT_EQUAL_UINT8(3U, estimate.m_usedAnchorCount);
    TEST_ASSERT_EQUAL_INT32(3000, static_cast<int32_t>(std::llround(
        estimate.m_positionXmm)));
    TEST_ASSERT_EQUAL_INT32(4000, static_cast<int32_t>(std::llround(
        estimate.m_positionYmm)));
    TEST_ASSERT_TRUE(estimate.m_residualRmsMm < 0.001);
}

/**
 * @brief ANCHOR数を増やしても同じAPIで最小二乗解を求められることを確認します。
 */
void TestFourAnchorsEstimateLeastSquaresPosition()
{
    const TagPositionAnchorMeasurement anchors[] = {
        MakeAnchor(1U, 0.0, 0.0, 5000.0),
        MakeAnchor(2U, 6000.0, 0.0, 5100.0),
        MakeAnchor(3U, 0.0, 8000.0, 4900.0),
        MakeAnchor(4U, 6000.0, 8000.0, 5000.0),
    };

    const TagPositionEstimate estimate = TagPositionEstimator::Estimate(
        anchors,
        sizeof(anchors) / sizeof(*anchors));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionEstimateStatus::Available),
        static_cast<int>(estimate.m_status));
    TEST_ASSERT_EQUAL_UINT8(4U, estimate.m_usedAnchorCount);
    TEST_ASSERT_INT32_WITHIN(
        1,
        2916,
        static_cast<int32_t>(std::llround(estimate.m_positionXmm)));
    TEST_ASSERT_INT32_WITHIN(
        1,
        4062,
        static_cast<int32_t>(std::llround(estimate.m_positionYmm)));
    TEST_ASSERT_TRUE(estimate.m_residualRmsMm > 0.0);
    TEST_ASSERT_TRUE(estimate.m_residualRmsMm < 2.0);
}

/**
 * @brief 3件未満では位置を決定せず不足状態を返すことを確認します。
 */
void TestInsufficientAnchorsAreRejected()
{
    const TagPositionAnchorMeasurement anchors[] = {
        MakeAnchor(1U, 0.0, 0.0, 1000.0),
        MakeAnchor(2U, 1000.0, 0.0, 1000.0),
    };

    const TagPositionEstimate estimate = TagPositionEstimator::Estimate(
        anchors,
        sizeof(anchors) / sizeof(*anchors));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionEstimateStatus::InsufficientAnchors),
        static_cast<int>(estimate.m_status));
    TEST_ASSERT_EQUAL_UINT8(2U, estimate.m_usedAnchorCount);
}

/**
 * @brief 全ANCHORが一直線の場合は退化配置として拒否することを確認します。
 */
void TestCollinearAnchorsAreRejected()
{
    const TagPositionAnchorMeasurement anchors[] = {
        MakeAnchor(1U, 0.0, 0.0, 1000.0),
        MakeAnchor(2U, 1000.0, 0.0, 1000.0),
        MakeAnchor(3U, 2000.0, 0.0, 1000.0),
        MakeAnchor(4U, 3000.0, 0.0, 1000.0),
    };

    const TagPositionEstimate estimate = TagPositionEstimator::Estimate(
        anchors,
        sizeof(anchors) / sizeof(*anchors));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionEstimateStatus::DegenerateGeometry),
        static_cast<int>(estimate.m_status));
    TEST_ASSERT_EQUAL_UINT8(4U, estimate.m_usedAnchorCount);
}

/**
 * @brief TAGのBtnA押下で測距一覧と位置グラフを相互切替できることを確認します。
 */
void TestTagButtonTogglesPositionGraph()
{
    TagPositionViewController controller;

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionDisplayPage::RangingResults),
        static_cast<int>(controller.GetPage()));
    TEST_ASSERT_FALSE(controller.Update(EnRunMode::Tag, false));
    TEST_ASSERT_TRUE(controller.Update(EnRunMode::Tag, true));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionDisplayPage::PositionGraph),
        static_cast<int>(controller.GetPage()));
    TEST_ASSERT_TRUE(controller.Update(EnRunMode::Tag, true));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionDisplayPage::RangingResults),
        static_cast<int>(controller.GetPage()));
}

/**
 * @brief ANCHORではBtnAを無視し位置グラフへ切り替えないことを確認します。
 */
void TestAnchorButtonDoesNotTogglePositionGraph()
{
    TagPositionViewController controller;

    TEST_ASSERT_FALSE(controller.Update(EnRunMode::Anchor, true));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionDisplayPage::RangingResults),
        static_cast<int>(controller.GetPage()));
}

/**
 * @brief TAGからANCHORへ変化した場合は測距一覧へ戻すことを確認します。
 */
void TestAnchorModeResetsPositionGraph()
{
    TagPositionViewController controller;
    TEST_ASSERT_TRUE(controller.Update(EnRunMode::Tag, true));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionDisplayPage::PositionGraph),
        static_cast<int>(controller.GetPage()));

    TEST_ASSERT_TRUE(controller.Update(EnRunMode::Anchor, false));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(EnTagPositionDisplayPage::RangingResults),
        static_cast<int>(controller.GetPage()));
}

/**
 * @brief TAG自己位置計算と画面切替testを実行します。
 *
 * @param argc 引数件数
 * @param argv 引数配列
 * @return Unity test結果
 */
int main(int argc, char** argv)
{
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(TestThreeAnchorsEstimateExactPosition);
    RUN_TEST(TestFourAnchorsEstimateLeastSquaresPosition);
    RUN_TEST(TestInsufficientAnchorsAreRejected);
    RUN_TEST(TestCollinearAnchorsAreRejected);
    RUN_TEST(TestTagButtonTogglesPositionGraph);
    RUN_TEST(TestAnchorButtonDoesNotTogglePositionGraph);
    RUN_TEST(TestAnchorModeResetsPositionGraph);
    return UNITY_END();
}
