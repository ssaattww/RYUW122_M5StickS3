#pragma once

#include <freertos/FreeRTOS.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "TaskTestRuntime.h"

using TaskFunction_t = void (*)(void*);

/**
 * @brief test用FreeRTOS task情報を表します。
 */
struct FakeTask
{
    TaskFunction_t m_function = nullptr;
    void* m_context = nullptr;
    std::string m_name;
    uint32_t m_stackSize = 0;
    UBaseType_t m_priority = 0;
    BaseType_t m_core = 0;
};

using TaskHandle_t = FakeTask*;

/**
 * @brief 1 cycle実行後にtask loopからtestへ戻す例外を表します。
 */
struct TaskTestYield
{
};

/**
 * @brief test用pin指定taskを作成します。
 *
 * @param function task entry
 * @param name task名
 * @param stackSize stack byte数
 * @param context task context
 * @param priority task優先度
 * @param handle 作成taskの格納先
 * @param core pin先core
 * @return 作成できた場合はpdPASS、それ以外はpdFALSE
 */
inline BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t function,
    const char* name,
    uint32_t stackSize,
    void* context,
    UBaseType_t priority,
    TaskHandle_t* handle,
    BaseType_t core)
{
    ++g_taskTestRuntime.m_taskCreateCallCount;
    const std::string taskName = name == nullptr ? "" : name;
    g_taskTestRuntime.m_events.emplace_back("create_task:" + taskName);
    if (g_taskTestRuntime.m_failTaskCreateCall ==
        g_taskTestRuntime.m_taskCreateCallCount)
    {
        if (handle != nullptr)
        {
            *handle = nullptr;
        }
        return pdFALSE;
    }
    auto* task = new FakeTask{
        function,
        context,
        taskName,
        stackSize,
        priority,
        core,
    };
    g_taskTestRuntime.m_tasks.push_back(task);
    if (handle != nullptr)
    {
        *handle = task;
    }
    return pdPASS;
}

/**
 * @brief test用taskを削除します。
 *
 * @param task 削除するtask
 */
inline void vTaskDelete(TaskHandle_t task)
{
    g_taskTestRuntime.m_events.emplace_back("delete_task:" + task->m_name);
    const auto iterator = std::find(
        g_taskTestRuntime.m_tasks.begin(),
        g_taskTestRuntime.m_tasks.end(),
        task);
    if (iterator != g_taskTestRuntime.m_tasks.end())
    {
        g_taskTestRuntime.m_tasks.erase(iterator);
    }
    delete task;
}

/**
 * @brief production task loopを1 cycleでtestへ戻します。
 *
 * @param ticks 未使用の待機tick
 */
inline void vTaskDelay(TickType_t ticks)
{
    static_cast<void>(ticks);
    throw TaskTestYield{};
}

/**
 * @brief 名前で選択したproduction task entryを1 cycle実行します。
 *
 * @param name 実行するtask名
 * @return 対象taskを実行できた場合はtrue
 */
inline bool RunTaskTestCycle(const char* name)
{
    for (FakeTask* task : g_taskTestRuntime.m_tasks)
    {
        if (task->m_name != name)
        {
            continue;
        }
        try
        {
            task->m_function(task->m_context);
        }
        catch (const TaskTestYield&)
        {
            return true;
        }
        return false;
    }
    return false;
}
