#pragma once

#include <cstdarg>
#include <cstdio>

#include "TaskTestRuntime.h"

/**
 * @brief production診断出力を記録するtest用Printです。
 */
class Print
{
public:
    /**
     * @brief 書式付き診断文字列をtest runtimeへ記録します。
     *
     * @param format printf互換書式
     * @return 記録した文字数
     */
    int printf(const char* format, ...)
    {
        char buffer[192]{};
        va_list arguments;
        va_start(arguments, format);
        const int length = vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);
        g_taskTestRuntime.m_serialText += buffer;
        ++g_taskTestRuntime.m_serialPrintfCount;
        g_taskTestRuntime.m_events.emplace_back("serial_printf");
        return length;
    }
};
