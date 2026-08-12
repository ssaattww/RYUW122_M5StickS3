#include <M5Unified.h>

#include "NtShell.h"
#include "PreferenceCommands.h"

namespace
{
    NtShell ntShell(Serial);
    PreferenceCommands preferenceCommands("ryuw122", "ryuw122_meta");
}

/**
 * @brief M5StackとNT-Shellを初期化します。
 */
void setup()
{
    M5.begin();
    Serial.begin(115200);
    preferenceCommands.Begin();
    ntShell.RegisterCommands(preferenceCommands.GetCommands());
    ntShell.Start();
}

/**
 * @brief M5Stackの入力状態を更新します。
 */
void loop()
{
    M5.update();
}
