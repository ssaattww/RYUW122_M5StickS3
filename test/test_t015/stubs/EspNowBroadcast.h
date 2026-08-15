#pragma once

#include "TaskTestRuntime.h"

/**
 * @brief test用NodeStatus broadcastを表します。
 */
class EspNowBroadcast
{
public:
    /** @brief broadcast更新順を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("broadcast");
    }
};
