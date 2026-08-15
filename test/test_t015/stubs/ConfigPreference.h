#pragma once

#include "RunMode.h"

/**
 * @brief production task controller用test設定名を提供します。
 */
class ConfigPreference
{
public:
    /**
     * @brief mode表示名を返します。
     *
     * @param mode 動作mode
     * @return mode表示名
     */
    static const char* GetModeName(EnRunMode mode)
    {
        return mode == EnRunMode::Tag ? "TAG" : "ANCHOR";
    }
};
