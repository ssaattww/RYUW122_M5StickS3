#pragma once

/**
 * @brief production task controller用の入力stubです。
 */
class TagPositionInput
{
public:
    /**
     * @brief testでは位置グラフ切替入力なしを返します。
     *
     * @return 常にfalse
     */
    static bool WasTogglePressed()
    {
        return false;
    }
};
