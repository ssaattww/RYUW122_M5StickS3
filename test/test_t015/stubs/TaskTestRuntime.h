#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct FakeQueue;
struct FakeTask;

/**
 * @brief production task controller用test runtimeの記録状態を表します。
 */
struct TaskTestRuntimeState
{
    int m_queueCreateCallCount = 0;
    int m_taskCreateCallCount = 0;
    int m_failQueueCreateCall = 0;
    int m_failTaskCreateCall = 0;
    int m_queueOverwriteCount = 0;
    int m_queueReceiveCount = 0;
    int m_canvasPushCount = 0;
    int m_m5UpdateCount = 0;
    uint32_t m_captureGeneration = 0;
    uint32_t m_drawnGeneration = 0;
    bool m_displayUpdateChanged = true;
    std::string m_canvasText;
    std::vector<std::string> m_events;
    std::vector<std::string> m_updateOrder;
    std::vector<FakeTask*> m_tasks;
};

extern TaskTestRuntimeState g_taskTestRuntime;

/**
 * @brief test runtimeの記録とfailure設定を初期化します。
 */
inline void ResetTaskTestRuntime()
{
    g_taskTestRuntime = TaskTestRuntimeState{};
}

/**
 * @brief test runtimeへ更新順を1件記録します。
 *
 * @param name 記録する更新名
 */
inline void RecordTaskTestUpdate(const char* name)
{
    g_taskTestRuntime.m_updateOrder.emplace_back(name);
}
