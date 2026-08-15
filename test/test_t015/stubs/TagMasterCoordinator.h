#pragma once

#include <cstdint>

#include "TaskTestRuntime.h"

/**
 * @brief test用TAGマスター選出を表します。
 */
class TagMasterCoordinator
{
public:
    /** @brief マスター選出更新順を記録します。 @param nowMs 現在millis */
    void Update(uint32_t nowMs)
    {
        static_cast<void>(nowMs);
        RecordTaskTestUpdate("master");
    }
};
