#pragma once

#include <freertos/FreeRTOS.h>

#include <cstddef>

struct TestQueue;
using QueueHandle_t = TestQueue*;

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize);
void vQueueDelete(QueueHandle_t queue);
BaseType_t xQueueSend(
    QueueHandle_t queue,
    const void* item,
    uint32_t waitTicks);
BaseType_t xQueuePeek(
    QueueHandle_t queue,
    void* item,
    uint32_t waitTicks);
BaseType_t xQueueReceive(
    QueueHandle_t queue,
    void* item,
    uint32_t waitTicks);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
