#include "TagPositionInput.h"

#include <M5Unified.h>

/**
 * @brief BtnAが新たに押されたか取得します。
 *
 * @return BtnA押下eventがある場合はtrue
 */
bool TagPositionInput::WasTogglePressed()
{
    return M5.BtnA.wasPressed();
}
