#pragma once

#include "TaskTestRuntime.h"

/**
 * @brief test用NTP時刻同期を表します。
 */
class NtpTimeSynchronizer
{
public:
    /** @brief NTP更新順を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("ntp");
    }
};
