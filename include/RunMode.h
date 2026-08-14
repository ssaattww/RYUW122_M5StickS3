#pragma once

#include <cstdint>

/**
 * @brief 端末の動作モードを表します。
 */
enum class EnRunMode : uint8_t
{
    Tag = 0,
    Anchor = 1,
};
