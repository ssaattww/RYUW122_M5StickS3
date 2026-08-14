#include <unity.h>

#include <cstring>

#include "SequentialRangingDisplay.h"

namespace
{
    /**
     * @brief 表示を正常初期化済み状態へ設定します。
     *
     * @param display 設定する表示管理
     */
    void SetHealthy(SequentialRangingDisplay& display)
    {
        display.SetInitializationHealth(
            EnRyuw122InitResult::Ok,
            true,
            true);
    }

    /**
     * @brief test用逐次測距結果を生成します。
     *
     * @param roundId ラウンドID
     * @param quality 時刻品質
     * @return test用逐次測距結果
     */
    TimedRangeMeasurement MakeMeasurement(
        uint32_t roundId,
        EnTimeQuality quality = EnTimeQuality::Synchronized)
    {
        TimedRangeMeasurement measurement{};
        measurement.roundId = roundId;
        measurement.anchorId = static_cast<uint8_t>(roundId);
        measurement.tagId = 2;
        measurement.status = EnRangeResultStatus::Success;
        measurement.distanceMm = roundId * 100U;
        measurement.uwbRssi = -70;
        measurement.rangingDurationUs = 1234;
        measurement.timeQuality = quality;
        return measurement;
    }

    /**
     * @brief test用ラウンド統計を生成します。
     *
     * @param roundId ラウンドID
     * @return test用ラウンド統計
     */
    SequentialRangeRoundSummary MakeSummary(uint32_t roundId)
    {
        SequentialRangeRoundSummary summary{};
        summary.roundId = roundId;
        summary.totalDurationUs = 5000;
        summary.expectedMeasurementCount = 4;
        summary.receivedMeasurementCount = 4;
        return summary;
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
 * @brief 空queueが再描画要求を生成しないことを確認します。
 */
void TestEmptyQueuesDoNotRequestDraw()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    M5Canvas canvas;
    SequentialRangingDisplay display(controller, broadcast, canvas);
    SetHealthy(display);

    TEST_ASSERT_FALSE(display.Update());
}

/**
 * @brief measurementとsummaryのburstを最新値まで1回でdrainすることを確認します。
 */
void TestBurstKeepsOnlyLatestMeasurementAndSummary()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    M5Canvas canvas;
    SequentialRangingDisplay display(controller, broadcast, canvas);
    SetHealthy(display);
    controller.PushMeasurement(MakeMeasurement(1));
    controller.PushMeasurement(MakeMeasurement(2));
    controller.PushSummary(MakeSummary(1));
    controller.PushSummary(MakeSummary(2));

    TEST_ASSERT_TRUE(display.Update());
    TEST_ASSERT_EQUAL_UINT32(0, controller.MeasurementRemaining());
    TEST_ASSERT_EQUAL_UINT32(0, controller.SummaryRemaining());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(35), "R2 A2-T2"));
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(71), "SUM R2"));
    TEST_ASSERT_FALSE(display.Update());
}

/**
 * @brief measurementだけの到着が最新測距表示を更新することを確認します。
 */
void TestMeasurementOnlyRequestsDraw()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    M5Canvas canvas;
    SequentialRangingDisplay display(controller, broadcast, canvas);
    SetHealthy(display);
    controller.PushMeasurement(MakeMeasurement(3));

    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(35), "R3 A3-T2"));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(71));
}

/**
 * @brief summaryだけの到着が最新ラウンド表示を更新することを確認します。
 */
void TestSummaryOnlyRequestsDraw()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    M5Canvas canvas;
    SequentialRangingDisplay display(controller, broadcast, canvas);
    SetHealthy(display);
    controller.PushSummary(MakeSummary(4));

    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(71), "SUM R4"));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(35));
}

/**
 * @brief 初期化失敗が通常再描画後も優先表示されることを確認します。
 */
void TestInitializationFailuresRemainVisible()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    M5Canvas canvas;
    SequentialRangingDisplay display(controller, broadcast, canvas);

    display.SetInitializationHealth(
        EnRyuw122InitResult::CommunicationFailed,
        true,
        true);
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("RYUW122: CommunicationFailed"));

    NodeStatus status{};
    status.nodeID = 7;
    broadcast.Inject(status);
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("RYUW122: CommunicationFailed"));
    TEST_ASSERT_FALSE(canvas.Contains("SEQ"));

    display.SetInitializationHealth(EnRyuw122InitResult::Ok, false, false);
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW transport failed"));
    controller.SetState(EnSequentialRangingState::ReadyToStart);
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW transport failed"));

    display.SetInitializationHealth(EnRyuw122InitResult::Ok, true, false);
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW broadcast failed"));
    broadcast.Inject(status);
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW broadcast failed"));
    display.Draw(EnRunMode::Anchor);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW broadcast failed"));
}

/**
 * @brief master identityとsession変更で旧表示と品質を破棄することを確認します。
 */
void TestMasterResetGenerationClearsPreviousSession()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    M5Canvas canvas;
    SequentialRangingDisplay display(controller, broadcast, canvas);
    SetHealthy(display);
    controller.SetState(EnSequentialRangingState::RunningRound);
    controller.PushMeasurement(MakeMeasurement(7));
    controller.PushSummary(MakeSummary(7));
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_TRUE(canvas.Contains("R7 A7-T2"));
    TEST_ASSERT_TRUE(canvas.Contains("Q:SYNC"));

    controller.SetState(EnSequentialRangingState::FollowingMaster);
    controller.AdvanceResetGeneration();
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "F FOLLOW Q:UNSYNC"));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(35));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(71));

    controller.PushMeasurement(MakeMeasurement(8));
    TEST_ASSERT_TRUE(display.Update());
    controller.SetState(EnSequentialRangingState::WaitingForSynchronization);
    controller.AdvanceResetGeneration();
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "M SYNC Q:UNSYNC"));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(35));

    controller.PushMeasurement(MakeMeasurement(9));
    TEST_ASSERT_TRUE(display.Update());
    controller.AdvanceResetGeneration();
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(35));

    controller.PushMeasurement(MakeMeasurement(10));
    TEST_ASSERT_TRUE(display.Update());
    controller.SetState(EnSequentialRangingState::WaitingForMaster);
    controller.AdvanceResetGeneration();
    TEST_ASSERT_TRUE(display.Update());
    display.Draw(EnRunMode::Tag);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "? WAIT Q:UNSYNC"));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(35));
}

/**
 * @brief T-008表示回帰testを実行します。
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
    RUN_TEST(TestEmptyQueuesDoNotRequestDraw);
    RUN_TEST(TestBurstKeepsOnlyLatestMeasurementAndSummary);
    RUN_TEST(TestMeasurementOnlyRequestsDraw);
    RUN_TEST(TestSummaryOnlyRequestsDraw);
    RUN_TEST(TestInitializationFailuresRemainVisible);
    RUN_TEST(TestMasterResetGenerationClearsPreviousSession);
    return UNITY_END();
}
