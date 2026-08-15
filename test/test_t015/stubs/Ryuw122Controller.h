#pragma once

#include "TaskTestRuntime.h"

/**
 * @brief test用RYUW122非同期測距を表します。
 */
class Ryuw122Controller
{
public:
    /** @brief RYUW122更新順を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("ryuw122");
    }
};
