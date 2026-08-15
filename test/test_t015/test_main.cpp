#include <unity.h>

#include <cstddef>
#include <cstring>

#include "EspNowBroadcast.h"
#include "EspNowReceiveQueueTerminator.h"
#include "EspNowTransport.h"
#include "NtpTimeSynchronizer.h"
#include "RangingDisplayTaskController.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"
#include "SequentialRangingDisplay.h"
#include "TagMasterCoordinator.h"
#include "TaskTestRuntime.h"

TaskTestRuntimeState g_taskTestRuntime{};
M5TestFacade M5{};

/**
 * @brief 各Unity test前の共通処理です。
 */
void setUp()
{
}

/**
 * @brief 各Unity test後の共通処理です。
 */
void tearDown()
{
}

namespace
{
    /**
     * @brief production task controllerとtest依存をまとめて保持します。
     */
    struct TaskControllerFixture
    {
        EspNowTransport m_transport;
        EspNowReceiveQueueTerminator m_terminator;
        EspNowBroadcast m_broadcast;
        TagMasterCoordinator m_master;
        NtpTimeSynchronizer m_ntp;
        Ryuw122Controller m_ryuw122;
        SequentialRangingController m_ranging;
        SequentialRangingDisplay m_display;
        M5Canvas m_canvas;
        RangingDisplayTaskController m_controller;

        /**
         * @brief test依存をproduction task controllerへ注入します。
         */
        TaskControllerFixture()
            : m_controller(
                  m_transport,
                  m_terminator,
                  m_broadcast,
                  m_master,
                  m_ntp,
                  m_ryuw122,
                  m_ranging,
                  m_display,
                  m_canvas)
        {
        }
    };

    /**
     * @brief task名に一致する作成済みtest taskを取得します。
     *
     * @param name 検索するtask名
     * @return 一致するtask。存在しない場合はnullptr
     */
    FakeTask* FindTask(const char* name)
    {
        for (FakeTask* task : g_taskTestRuntime.m_tasks)
        {
            if (task->m_name == name)
            {
                return task;
            }
        }
        return nullptr;
    }

    /**
     * @brief 記録済み更新順が期待値と一致することを確認します。
     *
     * @param expected 期待する更新名配列
     * @param count 配列要素数
     */
    void AssertUpdateOrder(const char* const* expected, size_t count)
    {
        TEST_ASSERT_EQUAL_UINT32(count, g_taskTestRuntime.m_updateOrder.size());
        for (size_t index = 0; index < count; ++index)
        {
            TEST_ASSERT_EQUAL_STRING(
                expected[index],
                g_taskTestRuntime.m_updateOrder[index].c_str());
        }
    }
}

/**
 * @brief production controllerが指定priorityとcoreで2 taskを開始することを確認します。
 */
void TestBeginCreatesConfiguredTasksAndInitialSnapshot()
{
    ResetTaskTestRuntime();
    TaskControllerFixture fixture;

    TEST_ASSERT_TRUE(fixture.m_controller.Begin());
    TEST_ASSERT_TRUE(fixture.m_controller.IsStarted());
    TEST_ASSERT_EQUAL_INT(1, g_taskTestRuntime.m_queueCreateCallCount);
    TEST_ASSERT_EQUAL_INT(1, g_taskTestRuntime.m_queueOverwriteCount);

    const FakeTask* ranging = FindTask("ranging");
    const FakeTask* display = FindTask("display");
    TEST_ASSERT_NOT_NULL(ranging);
    TEST_ASSERT_NOT_NULL(display);
    TEST_ASSERT_EQUAL_UINT32(
        RangingDisplayTaskController::m_rangingTaskPriority,
        ranging->m_priority);
    TEST_ASSERT_EQUAL_INT(
        RangingDisplayTaskController::m_rangingTaskCore,
        ranging->m_core);
    TEST_ASSERT_EQUAL_UINT32(
        RangingDisplayTaskController::m_displayTaskPriority,
        display->m_priority);
    TEST_ASSERT_EQUAL_INT(
        RangingDisplayTaskController::m_displayTaskCore,
        display->m_core);
    TEST_ASSERT_TRUE(ranging->m_priority > display->m_priority);

    fixture.m_controller.End();
}

/**
 * @brief 測距更新順、snapshot上書き、低優先度描画をproduction entryで確認します。
 */
void TestRangingOrderOverwriteAndDisplayLatestSnapshot()
{
    ResetTaskTestRuntime();
    TaskControllerFixture fixture;
    TEST_ASSERT_TRUE(fixture.m_controller.Begin());

    TEST_ASSERT_TRUE(RunTaskTestCycle("ranging"));
    const char* expectedOrder[] = {
        "transport",
        "terminator_begin",
        "broadcast",
        "master",
        "ntp",
        "ryuw122",
        "ranging",
        "terminator_end",
        "display",
    };
    AssertUpdateOrder(expectedOrder, sizeof(expectedOrder) / sizeof(*expectedOrder));
    TEST_ASSERT_TRUE(RunTaskTestCycle("ranging"));
    TEST_ASSERT_EQUAL_INT(3, g_taskTestRuntime.m_queueOverwriteCount);
    TEST_ASSERT_EQUAL_UINT32(3U, g_taskTestRuntime.m_captureGeneration);

    TEST_ASSERT_TRUE(RunTaskTestCycle("display"));
    TEST_ASSERT_EQUAL_INT(1, g_taskTestRuntime.m_queueReceiveCount);
    TEST_ASSERT_EQUAL_UINT32(3U, g_taskTestRuntime.m_drawnGeneration);
    TEST_ASSERT_EQUAL_INT(1, g_taskTestRuntime.m_m5UpdateCount);
    TEST_ASSERT_EQUAL_INT(1, g_taskTestRuntime.m_canvasPushCount);

    fixture.m_controller.End();
}

/**
 * @brief queue作成失敗を安全に終了して永続診断を描画できることを確認します。
 */
void TestQueueCreationFailureShowsPersistentDiagnostic()
{
    ResetTaskTestRuntime();
    g_taskTestRuntime.m_failQueueCreateCall = 1;
    TaskControllerFixture fixture;

    TEST_ASSERT_FALSE(fixture.m_controller.Begin());
    TEST_ASSERT_FALSE(fixture.m_controller.IsStarted());
    TEST_ASSERT_EQUAL_UINT32(0U, g_taskTestRuntime.m_tasks.size());
    fixture.m_controller.ShowTaskStartFailure();
    M5.update();
    TEST_ASSERT_NOT_NULL(strstr(
        g_taskTestRuntime.m_canvasText.c_str(),
        "TASK START FAILED"));
    TEST_ASSERT_EQUAL_INT(1, g_taskTestRuntime.m_canvasPushCount);
}

/**
 * @brief ranging task作成失敗時にsnapshot queueを解放することを確認します。
 */
void TestRangingTaskCreationFailureCleansQueue()
{
    ResetTaskTestRuntime();
    g_taskTestRuntime.m_failTaskCreateCall = 1;
    TaskControllerFixture fixture;

    TEST_ASSERT_FALSE(fixture.m_controller.Begin());
    TEST_ASSERT_FALSE(fixture.m_controller.IsStarted());
    TEST_ASSERT_EQUAL_UINT32(0U, g_taskTestRuntime.m_tasks.size());
    TEST_ASSERT_EQUAL_STRING(
        "delete_queue",
        g_taskTestRuntime.m_events.back().c_str());
}

/**
 * @brief display task作成失敗時に部分taskを停止後queueを解放し診断できることを確認します。
 */
void TestDisplayTaskCreationFailureCleansPartialTaskAndShowsDiagnostic()
{
    ResetTaskTestRuntime();
    g_taskTestRuntime.m_failTaskCreateCall = 2;
    TaskControllerFixture fixture;

    TEST_ASSERT_FALSE(fixture.m_controller.Begin());
    TEST_ASSERT_FALSE(fixture.m_controller.IsStarted());
    TEST_ASSERT_EQUAL_UINT32(0U, g_taskTestRuntime.m_tasks.size());
    const size_t count = g_taskTestRuntime.m_events.size();
    TEST_ASSERT_TRUE(count >= 2U);
    TEST_ASSERT_EQUAL_STRING(
        "delete_task:ranging",
        g_taskTestRuntime.m_events[count - 2U].c_str());
    TEST_ASSERT_EQUAL_STRING(
        "delete_queue",
        g_taskTestRuntime.m_events[count - 1U].c_str());

    fixture.m_controller.ShowTaskStartFailure();
    TEST_ASSERT_NOT_NULL(strstr(
        g_taskTestRuntime.m_canvasText.c_str(),
        "TASK START FAILED"));
}

/**
 * @brief Endがdisplay、ranging、snapshot queueの順で停止・解放することを確認します。
 */
void TestEndStopsTasksBeforeDeletingQueue()
{
    ResetTaskTestRuntime();
    TaskControllerFixture fixture;
    TEST_ASSERT_TRUE(fixture.m_controller.Begin());

    fixture.m_controller.End();
    const size_t count = g_taskTestRuntime.m_events.size();
    TEST_ASSERT_TRUE(count >= 3U);
    TEST_ASSERT_EQUAL_STRING(
        "delete_task:display",
        g_taskTestRuntime.m_events[count - 3U].c_str());
    TEST_ASSERT_EQUAL_STRING(
        "delete_task:ranging",
        g_taskTestRuntime.m_events[count - 2U].c_str());
    TEST_ASSERT_EQUAL_STRING(
        "delete_queue",
        g_taskTestRuntime.m_events[count - 1U].c_str());
    TEST_ASSERT_FALSE(fixture.m_controller.IsStarted());
}

/**
 * @brief T-015 production task controller testを実行します。
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
    RUN_TEST(TestBeginCreatesConfiguredTasksAndInitialSnapshot);
    RUN_TEST(TestRangingOrderOverwriteAndDisplayLatestSnapshot);
    RUN_TEST(TestQueueCreationFailureShowsPersistentDiagnostic);
    RUN_TEST(TestRangingTaskCreationFailureCleansQueue);
    RUN_TEST(TestDisplayTaskCreationFailureCleansPartialTaskAndShowsDiagnostic);
    RUN_TEST(TestEndStopsTasksBeforeDeletingQueue);
    return UNITY_END();
}
