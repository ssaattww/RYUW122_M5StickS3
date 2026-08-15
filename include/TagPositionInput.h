#pragma once

/**
 * @brief TAG位置グラフ切替に使用するM5入力を提供します。
 */
class TagPositionInput
{
public:
    /**
     * @brief BtnAが新たに押されたか取得します。
     *
     * @return BtnA押下eventがある場合はtrue
     */
    static bool WasTogglePressed();
};
