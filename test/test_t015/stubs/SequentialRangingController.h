#pragma once

#include "TaskTestRuntime.h"

/**
 * @brief test用逐次測距controllerを表します。
 */
class SequentialRangingController
{
public:
    /** @brief 逐次測距更新順を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("ranging");
    }
};
