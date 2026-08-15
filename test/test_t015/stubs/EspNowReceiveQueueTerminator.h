#pragma once

#include "TaskTestRuntime.h"

/**
 * @brief test用ESP-NOW受信FIFO最終所有者を表します。
 */
class EspNowReceiveQueueTerminator
{
public:
    /** @brief consumer更新前のcycle開始を記録します。 */
    void BeginCycle()
    {
        RecordTaskTestUpdate("terminator_begin");
    }

    /** @brief consumer更新後の最終所有処理を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("terminator_end");
    }
};
