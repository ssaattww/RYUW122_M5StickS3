#pragma once

#include "Ryuw122Initializer.h"

/**
 * @brief RYUW122の測距関連UART応答を固定長結果へ解析します。
 */
class Ryuw122ResponseParser
{
public:
    /**
     * @brief ANCHOR測距応答を距離単位と任意RSSIへ対応して解析します。
     *
     * @param line 解析する受信行
     * @param response 解析結果の格納先
     * @return 妥当なANCHOR測距応答の場合はtrue、それ以外はfalse
     */
    static bool ParseAnchorResponse(
        const char* line,
        Ryuw122PortResponse& response);

    /**
     * @brief +ERR応答からerror codeを解析します。
     *
     * @param line 解析する受信行
     * @param response 解析結果の格納先
     * @return 妥当な+ERR応答の場合はtrue、それ以外はfalse
     */
    static bool ParseErrorResponse(
        const char* line,
        Ryuw122PortResponse& response);

    /**
     * @brief 受信行がANCHOR測距応答prefixを持つか確認します。
     *
     * @param line 確認する受信行
     * @return ANCHOR測距応答prefixを持つ場合はtrue
     */
    static bool IsAnchorResponseLine(const char* line);
};
