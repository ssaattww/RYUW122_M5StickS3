#pragma once

#include <cstddef>
#include <cstring>

#include "TaskTestRuntime.h"

constexpr int TFT_BLACK = 0;
constexpr int TFT_WHITE = 1;
constexpr int TFT_RED = 2;
constexpr int TFT_DARKGREY = 3;

/**
 * @brief production task controller用test Canvasを表します。
 */
class M5Canvas
{
public:
    /** @brief test用sprite全体を塗りつぶします。 @param color 色 */
    void fillSprite(int color)
    {
        static_cast<void>(color);
        g_taskTestRuntime.m_canvasText.clear();
    }

    /** @brief test用矩形を塗りつぶします。 @param x X座標 @param y Y座標 @param width 幅 @param height 高さ @param color 色 */
    void fillRect(int x, int y, int width, int height, int color)
    {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(color);
    }

    /** @brief test用文字色を設定します。 @param color 色 */
    void setTextColor(int color)
    {
        static_cast<void>(color);
    }

    /** @brief test用文字倍率を設定します。 @param size 倍率 */
    void setTextSize(int size)
    {
        static_cast<void>(size);
    }

    /** @brief test用cursorを設定します。 @param x X座標 @param y Y座標 */
    void setCursor(int x, int y)
    {
        static_cast<void>(x);
        static_cast<void>(y);
    }

    /** @brief test用文字列を記録します。 @param text 文字列 @return 文字数 */
    size_t print(const char* text)
    {
        g_taskTestRuntime.m_canvasText += text;
        return strlen(text);
    }

    /** @brief test用Canvas幅を返します。 @return Canvas幅 */
    int width() const
    {
        return 135;
    }

    /** @brief test用文字列幅を返します。 @param text 文字列 @return pixel幅 */
    int textWidth(const char* text) const
    {
        return static_cast<int>(strlen(text) * 6U);
    }

    /** @brief test用sprite転送を記録します。 @param x X座標 @param y Y座標 */
    void pushSprite(int x, int y)
    {
        static_cast<void>(x);
        static_cast<void>(y);
        ++g_taskTestRuntime.m_canvasPushCount;
        g_taskTestRuntime.m_events.emplace_back("push_canvas");
    }
};

/**
 * @brief test用M5 facadeを表します。
 */
struct M5TestFacade
{
    /** @brief test用電源情報を表します。 */
    struct PowerFacade
    {
        /** @brief test用バッテリー残量を返します。 @return 残量 */
        int getBatteryLevel() const
        {
            return 80;
        }
    } Power;

    /** @brief M5入力更新を記録します。 */
    void update()
    {
        ++g_taskTestRuntime.m_m5UpdateCount;
        g_taskTestRuntime.m_events.emplace_back("m5_update");
    }
};

extern M5TestFacade M5;

/**
 * @brief test用millisを返します。
 *
 * @return 固定millis
 */
inline uint32_t millis()
{
    return 123U;
}
