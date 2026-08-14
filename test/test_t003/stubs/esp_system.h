#pragma once

#include <cstdint>

/**
 * @brief native test用の疑似乱数を返します。
 *
 * @return 32bit疑似乱数
 */
extern "C" uint32_t esp_random();
