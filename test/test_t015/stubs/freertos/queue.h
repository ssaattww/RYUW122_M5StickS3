#pragma once

#include <freertos/FreeRTOS.h>

#include <cstddef>
#include <cstring>
#include <vector>

#include "TaskTestRuntime.h"

/**
 * @brief test用FreeRTOS queueを表します。
 */
struct FakeQueue
{
    size_t m_itemSize = 0;
    size_t m_length = 0;
    size_t m_head = 0;
    size_t m_count = 0;
    std::vector<uint8_t> m_items;
};

using QueueHandle_t = FakeQueue*;

/**
 * @brief test用queueを作成します。
 *
 * @param length queue長
 * @param itemSize 1要素のbyte数
 * @return 作成したqueue。設定されたfailure時はnullptr
 */
inline QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize)
{
    ++g_taskTestRuntime.m_queueCreateCallCount;
    g_taskTestRuntime.m_events.emplace_back("create_queue");
    if (g_taskTestRuntime.m_failQueueCreateCall ==
            g_taskTestRuntime.m_queueCreateCallCount ||
        length == 0U)
    {
        return nullptr;
    }
    auto* queue = new FakeQueue{};
    queue->m_itemSize = itemSize;
    queue->m_length = length;
    queue->m_items.resize(static_cast<size_t>(length) * itemSize);
    return queue;
}

/**
 * @brief test用queueの最新要素を上書きします。
 *
 * @param queue 対象queue
 * @param item 保存する要素
 * @return 保存できた場合はpdTRUE
 */
inline BaseType_t xQueueOverwrite(QueueHandle_t queue, const void* item)
{
    if (queue == nullptr || item == nullptr)
    {
        return pdFALSE;
    }
    if (queue->m_length != 1U)
    {
        return pdFALSE;
    }
    memcpy(queue->m_items.data(), item, queue->m_itemSize);
    queue->m_head = 0U;
    queue->m_count = 1U;
    ++g_taskTestRuntime.m_queueOverwriteCount;
    g_taskTestRuntime.m_events.emplace_back("overwrite_queue");
    return pdTRUE;
}

/**
 * @brief test用FIFOへ要素を追加します。
 *
 * @param queue 対象queue
 * @param item 保存する要素
 * @param waitTicks 未使用の待機tick
 * @return 保存できた場合はpdTRUE
 */
inline BaseType_t xQueueSend(
    QueueHandle_t queue,
    const void* item,
    TickType_t waitTicks)
{
    static_cast<void>(waitTicks);
    if (queue == nullptr || item == nullptr || queue->m_count >= queue->m_length)
    {
        return pdFALSE;
    }
    const size_t tail = (queue->m_head + queue->m_count) % queue->m_length;
    memcpy(
        queue->m_items.data() + tail * queue->m_itemSize,
        item,
        queue->m_itemSize);
    ++queue->m_count;
    ++g_taskTestRuntime.m_queueSendCount;
    g_taskTestRuntime.m_events.emplace_back("send_queue");
    return pdTRUE;
}

/**
 * @brief test用queueから最新要素を取得します。
 *
 * @param queue 対象queue
 * @param item 取得先
 * @param waitTicks 未使用の待機tick
 * @return 取得できた場合はpdTRUE
 */
inline BaseType_t xQueueReceive(
    QueueHandle_t queue,
    void* item,
    TickType_t waitTicks)
{
    static_cast<void>(waitTicks);
    if (queue == nullptr || item == nullptr || queue->m_count == 0U)
    {
        return pdFALSE;
    }
    memcpy(
        item,
        queue->m_items.data() + queue->m_head * queue->m_itemSize,
        queue->m_itemSize);
    queue->m_head = (queue->m_head + 1U) % queue->m_length;
    --queue->m_count;
    ++g_taskTestRuntime.m_queueReceiveCount;
    g_taskTestRuntime.m_events.emplace_back("receive_queue");
    return pdTRUE;
}

/**
 * @brief test用queueを解放します。
 *
 * @param queue 解放するqueue
 */
inline void vQueueDelete(QueueHandle_t queue)
{
    g_taskTestRuntime.m_events.emplace_back("delete_queue");
    delete queue;
}
