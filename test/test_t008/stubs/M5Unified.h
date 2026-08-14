#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

constexpr int TFT_BLACK = 0;
constexpr int TFT_WHITE = 1;
constexpr int TFT_RED = 2;

/**
 * @brief test用の固定長Canvas描画記録を保持します。
 */
class M5Canvas
{
public:
    /**
     * @brief 指定サイズのtest用Canvasを生成します。
     *
     * @param width Canvas幅
     * @param height Canvas高さ
     */
    explicit M5Canvas(int width = 135, int height = 240)
        : m_width(width),
          m_height(height)
    {
    }

    /**
     * @brief Canvas幅を取得します。
     *
     * @return Canvas幅
     */
    int width() const
    {
        return m_width;
    }

    /**
     * @brief Canvas高さを取得します。
     *
     * @return Canvas高さ
     */
    int height() const
    {
        return m_height;
    }

    /**
     * @brief 指定範囲の記録済み文字列を消去します。
     *
     * @param x 左端座標
     * @param y 上端座標
     * @param width 消去幅
     * @param height 消去高さ
     * @param color 塗りつぶし色
     */
    void fillRect(int x, int y, int width, int height, int color)
    {
        static_cast<void>(x);
        static_cast<void>(width);
        static_cast<void>(color);
        for (Line& line : m_lines)
        {
            if (line.m_y >= y && line.m_y < y + height)
            {
                line = Line{};
            }
        }
    }

    /**
     * @brief testでは使用しない文字色を受け取ります。
     *
     * @param color 文字色
     */
    void setTextColor(int color)
    {
        static_cast<void>(color);
    }

    /**
     * @brief testでは使用しない文字倍率を受け取ります。
     *
     * @param size 文字倍率
     */
    void setTextSize(int size)
    {
        static_cast<void>(size);
    }

    /**
     * @brief 次の文字列を記録する座標を設定します。
     *
     * @param x X座標
     * @param y Y座標
     */
    void setCursor(int x, int y)
    {
        static_cast<void>(x);
        m_cursorY = y;
    }

    /**
     * @brief 現在座標へ文字列を記録します。
     *
     * @param text 記録する文字列
     * @return 記録した文字数
     */
    size_t print(const char* text)
    {
        Write(text);
        return strlen(text);
    }

    /**
     * @brief 現在座標へ書式付き文字列を記録します。
     *
     * @param format printf互換書式
     * @return 記録した文字数
     */
    int printf(const char* format, ...)
    {
        char buffer[96];
        va_list arguments;
        va_start(arguments, format);
        const int length = vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);
        Write(buffer);
        return length;
    }

    /**
     * @brief 指定Y座標へ最後に記録した文字列を取得します。
     *
     * @param y 検索するY座標
     * @return 記録済み文字列。存在しない場合は空文字列
     */
    const char* GetTextAtY(int y) const
    {
        for (const Line& line : m_lines)
        {
            if (line.m_y == y)
            {
                return line.m_text;
            }
        }
        return "";
    }

    /**
     * @brief 全描画行に指定文字列が含まれるか確認します。
     *
     * @param text 検索する文字列
     * @return 含まれる場合はtrue、それ以外はfalse
     */
    bool Contains(const char* text) const
    {
        for (const Line& line : m_lines)
        {
            if (strstr(line.m_text, text) != nullptr)
            {
                return true;
            }
        }
        return false;
    }

private:
    /**
     * @brief 1行分の描画記録を表します。
     */
    struct Line
    {
        int m_y = -1;
        char m_text[96]{};
    };

    /**
     * @brief 現在座標へ文字列を上書きします。
     *
     * @param text 記録する文字列
     */
    void Write(const char* text)
    {
        Line* target = nullptr;
        for (Line& line : m_lines)
        {
            if (line.m_y == m_cursorY)
            {
                target = &line;
                break;
            }
            if (target == nullptr && line.m_y < 0)
            {
                target = &line;
            }
        }
        if (target == nullptr)
        {
            return;
        }
        target->m_y = m_cursorY;
        strncpy(target->m_text, text, sizeof(target->m_text) - 1U);
        target->m_text[sizeof(target->m_text) - 1U] = '\0';
    }

    int m_width;
    int m_height;
    int m_cursorY = 0;
    Line m_lines[20]{};
};
