#include <M5Unified.h>

#include "NtShell.h"

#include "PreferenceCommands.h"
#include "ConfigPreference.h"
#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "EspNowReceiveQueueTerminator.h"
#include "EspNowTransport.h"
#include "NvsPreferenceStore.h"
#include "NtpTimeSynchronizer.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"
#include "SequentialRangingDisplay.h"
#include "SequentialRangingProtocolCodec.h"
#include "TagMasterCoordinator.h"

namespace
{
    NtShell ntShell(Serial);
    NvsPreferenceStore preferenceStore("ryuw122", "ryuw122_meta");
    PreferenceCommands preferenceCommands(preferenceStore);
    ConfigPreference configPreference(preferenceStore);
    ConfigRuntime configRuntime;
    EspNowTransport espNowTransport;
    EspNowBroadcast espNowBroadcast(espNowTransport, configRuntime);
    TagMasterCoordinator tagMasterCoordinator(espNowBroadcast);
    NtpTimeSynchronizer ntpTimeSynchronizer(
        espNowTransport,
        espNowBroadcast,
        tagMasterCoordinator,
        configRuntime);
    Ryuw122Controller ryuw122Controller(Serial1, configRuntime);
    M5Canvas canvas(&M5.Display);
    SequentialRangingProtocolCodec sequentialRangingProtocolCodec;
    SequentialRangingController sequentialRangingController(
        espNowTransport,
        espNowBroadcast,
        tagMasterCoordinator,
        ntpTimeSynchronizer,
        ryuw122Controller,
        sequentialRangingProtocolCodec);
    EspNowReceiveQueueTerminator espNowReceiveQueueTerminator(
        espNowTransport);
    SequentialRangingDisplay sequentialRangingDisplay(
        sequentialRangingController,
        espNowBroadcast,
        canvas);

    constexpr int StatusBarHeight = 20;

    /**
     * @brief ノードID、動作モード、バッテリー残量をステータスバーへ描画します。
     *
     * @param mode 表示する動作モード
     * @param nodeId 表示するノードID
     */
    void DrawStatus(EnRunMode mode, uint8_t nodeId)
    {
        // 背景
        canvas.fillRect(
            0,
            0,
            canvas.width(),
            StatusBarHeight,
            TFT_DARKGREY);

        canvas.setTextColor(TFT_WHITE);
        canvas.setTextSize(1);

        // 左側: Node ID / Mode
        char statusText[20];
        snprintf(
            statusText,
            sizeof(statusText),
            "ID:%u %s",
            nodeId,
            ConfigPreference::GetModeName(mode));
        canvas.setCursor(4, 6);
        canvas.print(statusText);

        // 右側: Battery
        int battery = M5.Power.getBatteryLevel();

        char text[8];
        snprintf(text, sizeof(text), "%d%%", battery);

        int textWidth = canvas.textWidth(text);

        canvas.setCursor(
            canvas.width() - textWidth - 4,
            6);

        canvas.print(text);
    }

};

/**
 * @brief M5Stack、通信、時刻同期、逐次測距、画面、NT-Shellを初期化します。
 */
void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    canvas.createSprite(
        M5.Display.width(),
        M5.Display.height());

    Serial.begin(115200);

    // nvsの設定
    preferenceStore.Begin();
    configRuntime.Init(configPreference);

    const EnRyuw122InitResult ryuw122Result = ryuw122Controller.Begin();
    const bool transportStarted = espNowTransport.Begin(
        configRuntime.GetCurrentEspnowChannel(),
        configRuntime.GetWifiPowerSave());
    const bool broadcastStarted =
        transportStarted && espNowBroadcast.Begin();
    const bool espNowStarted = transportStarted && broadcastStarted;
    if (espNowStarted)
    {
        tagMasterCoordinator.Begin(millis());
    }
    if (espNowStarted && ryuw122Result == EnRyuw122InitResult::Ok)
    {
        sequentialRangingController.Begin();
    }
    sequentialRangingDisplay.SetInitializationHealth(
        ryuw122Result,
        transportStarted,
        broadcastStarted);
    sequentialRangingDisplay.Update();

    canvas.fillSprite(TFT_BLACK);
    DrawStatus(
        configRuntime.GetRunMode(),
        configRuntime.GetCurrentNodeID());
    sequentialRangingDisplay.Draw(configRuntime.GetRunMode());
    canvas.pushSprite(0, 0);

    ntShell.RegisterCommands(preferenceCommands.GetCommands());
    ntShell.Start();

}

/**
 * @brief 入力、通信、同期、UWB測距、逐次測距、画面を順番に更新します。
 */
void loop()
{
    M5.update();
    bool canvasChanged = false;
    if (M5.BtnA.isPressed())
    {
        EnRunMode runmode = configRuntime.GetRunMode();
        EnRunMode nextMode = (runmode == EnRunMode::Tag) ? EnRunMode::Anchor : EnRunMode::Tag;
        configRuntime.SetRunMode(nextMode);
        DrawStatus(
            nextMode,
            configRuntime.GetCurrentNodeID());
        canvasChanged = true;
    }

    espNowTransport.Update();
    // transport更新後の削除件数を既知consumer処理前に固定する。
    espNowReceiveQueueTerminator.BeginCycle();
    espNowBroadcast.Update();
    tagMasterCoordinator.Update(millis());
    ntpTimeSynchronizer.Update();
    ryuw122Controller.Update();
    sequentialRangingController.Update();
    // 既知consumerがFIFOを進めなかったcycleだけ未所有packetを1件破棄する。
    espNowReceiveQueueTerminator.Update();
    canvasChanged = sequentialRangingDisplay.Update() || canvasChanged;

    if (canvasChanged)
    {
        DrawStatus(
            configRuntime.GetRunMode(),
            configRuntime.GetCurrentNodeID());
        sequentialRangingDisplay.Draw(configRuntime.GetRunMode());
        canvas.pushSprite(0, 0);
    }

    delay(1);
}
