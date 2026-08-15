#pragma once

#include "TaskTestRuntime.h"

/**
 * @brief test用ESP-NOW transportを表します。
 */
class EspNowTransport
{
public:
    /** @brief transport更新順を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("transport");
    }
};
