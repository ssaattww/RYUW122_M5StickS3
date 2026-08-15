#pragma once

#include "M5Unified.h"
#include "SequentialRangingDisplay.h"

/**
 * @brief production task controller用の位置グラフ描画stubです。
 */
class TagPositionGraphRenderer
{
public:
    /**
     * @brief testでは位置グラフ描画を無効にします。
     *
     * @param snapshot 確認対象snapshot
     * @return 常にfalse
     */
    static bool CanDraw(
        const SequentialRangingDisplaySnapshot& snapshot)
    {
        static_cast<void>(snapshot);
        return false;
    }

    /**
     * @brief testでは描画を行いません。
     *
     * @param snapshot 描画対象snapshot
     * @param canvas 描画先Canvas
     */
    static void Draw(
        const SequentialRangingDisplaySnapshot& snapshot,
        M5Canvas& canvas)
    {
        static_cast<void>(snapshot);
        static_cast<void>(canvas);
    }
};
