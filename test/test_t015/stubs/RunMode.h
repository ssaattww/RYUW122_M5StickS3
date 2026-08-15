#pragma once

#include <cstdint>

/**
 * @brief test用動作modeを表します。
 */
enum class EnRunMode : uint8_t
{
    Tag = 0,
    Anchor = 1,
};
