#include <M5Unified.h>

void setup()
{
    M5.begin();
    M5.Display.fillScreen(TFT_BLUE);
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed())
    {
        M5.Display.fillScreen(TFT_YELLOW);
    }
    if (M5.BtnB.wasPressed())
    {
        M5.Display.fillScreen(M5.Display.color565(255, 75, 0)); // 朱色
    }
}