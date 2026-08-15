#include <unity.h>

#include <cstdint>
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
     * @brief 現在の表示modelをsnapshot化して描画します。
     *
     * @param display 描画する表示管理
     */
    void DrawCurrent(SequentialRangingDisplay& display)
    {
        SequentialRangingDisplaySnapshot snapshot{};
        display.CaptureSnapshot(snapshot);
        display.Draw(snapshot);
    }

    /**
     * @brief test用自ノード状態を設定します。
     *
     * @param broadcast 設定対象broadcast
     * @param nodeId 自ノードID
     * @param mode 自ノードmode
     */
    void SetLocalNode(
        EspNowBroadcast& broadcast,
        uint8_t nodeId,
        EnRunMode mode)
    {
        NodeStatus status{};
        status.nodeID = nodeId;
        status.mode = mode;
        broadcast.SetLocalStatus(status);
    }

    /**
     * @brief test用逐次測距結果を生成します。
     *
     * @param anchorId ANCHOR ID
     * @param tagId TAG ID
     * @param distanceMm 距離
     * @param completedSecond マスターTAG基準完了秒
     * @param status 測距結果状態
     * @param quality 時刻品質
     * @return test用逐次測距結果
     */
    TimedRangeMeasurement MakeMeasurement(
        uint8_t anchorId,
        uint8_t tagId,
        uint32_t distanceMm,
        uint64_t completedSecond,
        EnRangeResultStatus status = EnRangeResultStatus::Success,
        EnTimeQuality quality = EnTimeQuality::Synchronized)
    {
        TimedRangeMeasurement measurement{};
        measurement.roundId = completedSecond == 0U
            ? 0U
            : static_cast<uint32_t>(completedSecond);
        measurement.anchorId = anchorId;
        measurement.tagId = tagId;
        measurement.status = status;
        measurement.distanceMm = distanceMm;
        measurement.uwbRssi = -70;
        measurement.rangingCompletedMasterTimeUs =
            completedSecond * 1000000U;
        measurement.rangingDurationUs = 1234;
        measurement.timeQuality = quality;
        return measurement;
    }

    /**
     * @brief test用ラウンド統計を生成します。
     *
     * @return test用ラウンド統計
     */
    SequentialRangeRoundSummary MakeSummary()
    {
        SequentialRangeRoundSummary summary{};
        summary.roundId = 4;
        summary.totalDurationUs = 5000;
        summary.expectedMeasurementCount = 4;
        summary.receivedMeasurementCount = 4;
        return summary;
    }

    /**
     * @brief test用NodeStatusを生成します。
     *
     * @param nodeId ノードID
     * @param mode 動作モード
     * @return test用NodeStatus
     */
    NodeStatus MakeNodeStatus(uint8_t nodeId, EnRunMode mode)
    {
        NodeStatus status{};
        status.nodeID = nodeId;
        status.mode = mode;
        status.anchorPositionX = static_cast<uint16_t>(nodeId * 10U);
        status.anchorPositionY = static_cast<uint16_t>(nodeId * 20U);
        return status;
    }

    /**
     * @brief 指定行が135 pixel幅へ収まることを確認します。
     *
     * @param canvas 描画結果Canvas
     * @param y 確認するY座標
     */
    void AssertLineFits(const M5Canvas& canvas, int y)
    {
        TEST_ASSERT_TRUE(strlen(canvas.GetTextAtY(y)) > 0U);
        TEST_ASSERT_TRUE(canvas.GetTextRightAtY(y) <= canvas.width());
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
 * @brief 空queueと未同期時刻が再描画要求を生成しないことを確認します。
 */
void TestEmptyQueuesDoNotRequestDraw()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);

    TEST_ASSERT_FALSE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_EQUAL_STRING("NOW UNSYNC", canvas.GetTextAtY(35));
}

/**
 * @brief burstをdrainしてANCHOR別最新値をID昇順に保持することを確認します。
 */
void TestBurstKeepsLatestPerAnchorInIdOrder()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.PushMeasurement(MakeMeasurement(3, 2, 300, 103));
    controller.PushMeasurement(MakeMeasurement(1, 2, 100, 101));
    controller.PushMeasurement(MakeMeasurement(2, 2, 200, 102));
    controller.PushMeasurement(MakeMeasurement(1, 2, 999, 104));
    controller.PushSummary(MakeSummary());
    controller.PushSummary(MakeSummary());

    TEST_ASSERT_TRUE(display.Update());
    TEST_ASSERT_EQUAL_UINT32(0, controller.MeasurementRemaining());
    TEST_ASSERT_EQUAL_UINT32(0, controller.SummaryRemaining());
    DrawCurrent(display);
    TEST_ASSERT_EQUAL_STRING(
        "A1 999mm@000104s",
        canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_STRING(
        "A2 200mm@000102s",
        canvas.GetTextAtY(59));
    TEST_ASSERT_EQUAL_STRING(
        "A3 300mm@000103s",
        canvas.GetTextAtY(71));
    TEST_ASSERT_FALSE(canvas.Contains("SUM"));
    TEST_ASSERT_FALSE(display.Update());
}

/**
 * @brief master TAGが他TAG向け結果を一覧へ入れないことを確認します。
 */
void TestMasterTagFiltersMeasurementsForOtherTags()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.SetState(EnSequentialRangingState::RunningRound);
    controller.PushMeasurement(MakeMeasurement(4, 2, 444, 201));
    controller.PushMeasurement(MakeMeasurement(4, 3, 999, 202));
    controller.PushMeasurement(MakeMeasurement(5, 3, 555, 203));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "M RUN"));
    TEST_ASSERT_EQUAL_STRING(
        "A4 444mm@000201s",
        canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(59));
    TEST_ASSERT_FALSE(canvas.Contains("999mm"));
}

/**
 * @brief follower TAGが自TAGへ転送された結果と現在時刻を描画することを確認します。
 */
void TestFollowerTagDrawsForwardedMeasurementAndCurrentTime()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    timeSynchronizer.SetCurrentMasterTime(
        uint64_t{1234567} * 1000000U);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.SetState(EnSequentialRangingState::FollowingMaster);
    controller.PushMeasurement(MakeMeasurement(6, 2, 600, 1234500));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "F FOLLOW"));
    TEST_ASSERT_EQUAL_STRING("NOW 234567s", canvas.GetTextAtY(35));
    TEST_ASSERT_EQUAL_STRING(
        "A6 600mm@234500s",
        canvas.GetTextAtY(47));
}

/**
 * @brief 計測時刻の品質と0値を独立に判定して表示することを確認します。
 */
void TestMeasurementTimeValidityAndZeroValue()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);

    controller.PushMeasurement(MakeMeasurement(
        1, 2, 1, 0,
        EnRangeResultStatus::Success,
        EnTimeQuality::Synchronized));
    controller.PushMeasurement(MakeMeasurement(
        2, 2, 2, 0,
        EnRangeResultStatus::Success,
        EnTimeQuality::PowerSaveEnabled));
    controller.PushMeasurement(MakeMeasurement(
        3, 2, 3, 3,
        EnRangeResultStatus::Success,
        EnTimeQuality::ReceiveTimestampUnavailable));
    controller.PushMeasurement(MakeMeasurement(
        4, 2, 4, 4,
        EnRangeResultStatus::Success,
        EnTimeQuality::SynchronizationExpired));
    controller.PushMeasurement(MakeMeasurement(
        5, 2, 5, 0,
        EnRangeResultStatus::Success,
        EnTimeQuality::Unsynchronized));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_EQUAL_STRING(
        "A1 1mm@000000s",
        canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_STRING(
        "A2 2mm@000000s",
        canvas.GetTextAtY(59));
    TEST_ASSERT_EQUAL_STRING(
        "A3 3mm@000003s",
        canvas.GetTextAtY(71));
    TEST_ASSERT_EQUAL_STRING("A4 4mm@UNSYNC", canvas.GetTextAtY(83));
    TEST_ASSERT_EQUAL_STRING("A5 5mm@UNSYNC", canvas.GetTextAtY(95));
}

/**
 * @brief NOWと計測時刻が同じ6桁秒境界で折り返すことを確認します。
 */
void TestMasterTimeModuloBoundaryIsShared()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    timeSynchronizer.SetCurrentMasterTime(
        uint64_t{1000000} * 1000000U);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.PushMeasurement(MakeMeasurement(
        1, 2, 1, uint64_t{999999}));
    controller.PushMeasurement(MakeMeasurement(
        2, 2, 2, uint64_t{1000000}));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_EQUAL_STRING("NOW 000000s", canvas.GetTextAtY(35));
    TEST_ASSERT_EQUAL_STRING(
        "A1 1mm@999999s",
        canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_STRING(
        "A2 2mm@000000s",
        canvas.GetTextAtY(59));
}

/**
 * @brief 最大ANCHOR IDのtimeoutと6桁時刻が135 pixel幅へ収まることを確認します。
 */
void TestMaximumTimeoutLineFitsWidth()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas(135, 240);
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.PushMeasurement(MakeMeasurement(
        255,
        2,
        UINT32_MAX,
        uint64_t{999999},
        EnRangeResultStatus::TimedOut));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_EQUAL_STRING(
        "A255 TIMEOUT@999999s",
        canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_INT(124, canvas.GetTextRightAtY(47));
    AssertLineFits(canvas, 47);
}

/**
 * @brief 現在マスター時刻の秒変化と無効化だけが再描画を要求することを確認します。
 */
void TestCurrentMasterTimeRefreshesOncePerSecond()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);

    timeSynchronizer.SetCurrentMasterTime(1000000U);
    TEST_ASSERT_TRUE(display.Update());
    timeSynchronizer.SetCurrentMasterTime(1500000U);
    TEST_ASSERT_FALSE(display.Update());
    timeSynchronizer.SetCurrentMasterTime(2000000U);
    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_EQUAL_STRING("NOW 000002s", canvas.GetTextAtY(35));

    timeSynchronizer.ClearCurrentMasterTime();
    TEST_ASSERT_TRUE(display.Update());
    TEST_ASSERT_FALSE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_EQUAL_STRING("NOW UNSYNC", canvas.GetTextAtY(35));
}

/**
 * @brief 8 ANCHOR結果とNodeStatus 3件が画面内かつ135 pixel幅内へ収まることを確認します。
 */
void TestEightAnchorResultsAndThreeNodesFitScreen()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas(135, 240);
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    timeSynchronizer.SetCurrentMasterTime(
        uint64_t{9999999999} * 1000000U);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);

    controller.PushMeasurement(MakeMeasurement(
        255, 2, UINT32_MAX, 9999999999));
    controller.PushMeasurement(MakeMeasurement(
        200, 2, 99999999U, 9999999998));
    controller.PushMeasurement(MakeMeasurement(
        100, 2, 0, 9999999997, EnRangeResultStatus::Unreachable));
    controller.PushMeasurement(MakeMeasurement(
        50, 2, 0, 9999999996, EnRangeResultStatus::TimedOut));
    controller.PushMeasurement(MakeMeasurement(
        8, 2, 0, 9999999995, EnRangeResultStatus::Failed));
    controller.PushMeasurement(MakeMeasurement(
        7, 2, 99999U, 9999999994));
    controller.PushMeasurement(MakeMeasurement(
        2, 2, 12345U, 9999999993));
    controller.PushMeasurement(MakeMeasurement(
        1, 2, 1U, 9999999992));
    controller.PushMeasurement(MakeMeasurement(
        9, 2, 9U, 999991));
    broadcast.Inject(MakeNodeStatus(1, EnRunMode::Tag));
    broadcast.Inject(MakeNodeStatus(2, EnRunMode::Anchor));
    broadcast.Inject(MakeNodeStatus(3, EnRunMode::Tag));
    broadcast.Inject(MakeNodeStatus(4, EnRunMode::Anchor));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_EQUAL_STRING("NOW 999999s", canvas.GetTextAtY(35));
    TEST_ASSERT_EQUAL_STRING(
        "A1 1mm@999992s",
        canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_STRING(
        "A2 12345mm@999993s",
        canvas.GetTextAtY(59));
    TEST_ASSERT_EQUAL_STRING(
        "A7 99999mm@999994s",
        canvas.GetTextAtY(71));
    TEST_ASSERT_EQUAL_STRING(
        "A8 FAIL@999995s",
        canvas.GetTextAtY(83));
    TEST_ASSERT_EQUAL_STRING(
        "A50 TIMEOUT@999996s",
        canvas.GetTextAtY(95));
    TEST_ASSERT_EQUAL_STRING(
        "A100 MISS@999997s",
        canvas.GetTextAtY(107));
    TEST_ASSERT_EQUAL_STRING(
        "A200 99999m@999998s",
        canvas.GetTextAtY(119));
    TEST_ASSERT_EQUAL_STRING(
        "A255 4294km@999999s",
        canvas.GetTextAtY(131));
    TEST_ASSERT_FALSE(canvas.Contains("A9 9mm"));
    TEST_ASSERT_EQUAL_STRING("ID MODE X,Y", canvas.GetTextAtY(143));
    TEST_ASSERT_EQUAL_STRING("1 T 10,20", canvas.GetTextAtY(155));
    TEST_ASSERT_EQUAL_STRING("2 A 20,40", canvas.GetTextAtY(167));
    TEST_ASSERT_EQUAL_STRING("3 T 30,60", canvas.GetTextAtY(179));
    TEST_ASSERT_FALSE(canvas.Contains("4 A 40,80"));

    const int lineCoordinates[] = {
        23, 35, 47, 59, 71, 83, 95,
        107, 119, 131, 143, 155, 167, 179,
    };
    for (const int y : lineCoordinates)
    {
        AssertLineFits(canvas, y);
    }
    TEST_ASSERT_TRUE(179 + 8 <= canvas.height());
}

/**
 * @brief ANCHORではTAG専用一覧と現在マスター時刻を描画しないことを確認します。
 */
void TestAnchorOmitsTagResultsAndCurrentTime()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 8, EnRunMode::Anchor);
    timeSynchronizer.SetCurrentMasterTime(
        uint64_t{1234567} * 1000000U);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.SetState(EnSequentialRangingState::AnchorRanging);
    controller.PushMeasurement(MakeMeasurement(7, 8, 700, 700));
    broadcast.Inject(MakeNodeStatus(9, EnRunMode::Anchor));

    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);

    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "A RANGE Q:UNSYNC"));
    TEST_ASSERT_FALSE(canvas.Contains("NOW"));
    TEST_ASSERT_FALSE(canvas.Contains("A7 700mm"));
    TEST_ASSERT_EQUAL_STRING("ID MODE X,Y", canvas.GetTextAtY(143));
    TEST_ASSERT_EQUAL_STRING("9 A 90,180", canvas.GetTextAtY(155));
}

/**
 * @brief 初期化失敗が通常再描画後も優先表示されることを確認します。
 */
void TestInitializationFailuresRemainVisible()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);

    display.SetInitializationHealth(
        EnRyuw122InitResult::CommunicationFailed,
        true,
        true);
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("RYUW122: CommunicationFailed"));

    const NodeStatus status = MakeNodeStatus(7, EnRunMode::Tag);
    broadcast.Inject(status);
    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("RYUW122: CommunicationFailed"));
    TEST_ASSERT_FALSE(canvas.Contains("SEQ"));

    display.SetInitializationHealth(EnRyuw122InitResult::Ok, false, false);
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW transport failed"));
    controller.SetState(EnSequentialRangingState::ReadyToStart);
    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW transport failed"));

    display.SetInitializationHealth(EnRyuw122InitResult::Ok, true, false);
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW broadcast failed"));
    broadcast.Inject(status);
    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("ESP-NOW broadcast failed"));
}

/**
 * @brief master session変更で全ANCHOR結果と表示品質を破棄することを確認します。
 */
void TestMasterResetGenerationClearsAnchorResults()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Tag);
    timeSynchronizer.SetCurrentMasterTime(1000000U);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);
    controller.SetState(EnSequentialRangingState::RunningRound);
    controller.PushMeasurement(MakeMeasurement(1, 2, 100, 1));
    controller.PushMeasurement(MakeMeasurement(2, 2, 200, 2));
    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_TRUE(canvas.Contains("A1 100mm"));
    TEST_ASSERT_TRUE(canvas.Contains("A2 200mm"));
    TEST_ASSERT_TRUE(canvas.Contains("Q:SYNC"));

    controller.SetState(EnSequentialRangingState::FollowingMaster);
    controller.AdvanceResetGeneration();
    TEST_ASSERT_TRUE(display.Update());
    DrawCurrent(display);
    TEST_ASSERT_NOT_NULL(strstr(
        canvas.GetTextAtY(23),
        "F FOLLOW Q:UNSYNC"));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(47));
    TEST_ASSERT_EQUAL_STRING("", canvas.GetTextAtY(59));
    TEST_ASSERT_EQUAL_STRING("NOW 000001s", canvas.GetTextAtY(35));
}

/**
 * @brief 取得済みsnapshotが後続の測距model更新から独立して描画できることを確認します。
 */
void TestCapturedSnapshotIsIndependentFromLaterRangingUpdates()
{
    SequentialRangingController controller;
    EspNowBroadcast broadcast;
    NtpTimeSynchronizer timeSynchronizer;
    M5Canvas canvas;
    SetLocalNode(broadcast, 2, EnRunMode::Anchor);
    SequentialRangingDisplay display(
        controller,
        broadcast,
        timeSynchronizer,
        canvas);
    SetHealthy(display);

    controller.SetState(EnSequentialRangingState::AnchorRanging);
    TEST_ASSERT_TRUE(display.Update());
    SequentialRangingDisplaySnapshot rangingSnapshot{};
    display.CaptureSnapshot(rangingSnapshot);

    controller.SetState(EnSequentialRangingState::AnchorIdle);
    TEST_ASSERT_TRUE(display.Update());
    SequentialRangingDisplaySnapshot idleSnapshot{};
    display.CaptureSnapshot(idleSnapshot);

    display.Draw(rangingSnapshot);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "A RANGE"));
    display.Draw(idleSnapshot);
    TEST_ASSERT_NOT_NULL(strstr(canvas.GetTextAtY(23), "A IDLE"));
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
    RUN_TEST(TestBurstKeepsLatestPerAnchorInIdOrder);
    RUN_TEST(TestMasterTagFiltersMeasurementsForOtherTags);
    RUN_TEST(TestFollowerTagDrawsForwardedMeasurementAndCurrentTime);
    RUN_TEST(TestMeasurementTimeValidityAndZeroValue);
    RUN_TEST(TestMasterTimeModuloBoundaryIsShared);
    RUN_TEST(TestMaximumTimeoutLineFitsWidth);
    RUN_TEST(TestCurrentMasterTimeRefreshesOncePerSecond);
    RUN_TEST(TestEightAnchorResultsAndThreeNodesFitScreen);
    RUN_TEST(TestAnchorOmitsTagResultsAndCurrentTime);
    RUN_TEST(TestInitializationFailuresRemainVisible);
    RUN_TEST(TestMasterResetGenerationClearsAnchorResults);
    RUN_TEST(TestCapturedSnapshotIsIndependentFromLaterRangingUpdates);
    return UNITY_END();
}
