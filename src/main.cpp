#include <M5Unified.h>

#include "BuildOptions.h"
#include "NtShell.h"

#include "PreferenceCommands.h"
#include "ConfigPreference.h"
#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "EspNowReceiveQueueTerminator.h"
#include "EspNowTransport.h"
#include "NvsPreferenceStore.h"
#include "NtpTimeSynchronizer.h"
#include "RangingDisplayTaskController.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"
#include "SequentialRangingDisplay.h"
#include "SequentialRangingProtocolCodec.h"
#include "TagMasterCoordinator.h"

namespace
{
#if NT_SHELL_ENABLED
    NtShell ntShell(Serial);
#endif
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
        ntpTimeSynchronizer,
        canvas);
    RangingDisplayTaskController rangingDisplayTaskController(
        espNowTransport,
        espNowReceiveQueueTerminator,
        espNowBroadcast,
        tagMasterCoordinator,
        ntpTimeSynchronizer,
        ryuw122Controller,
        sequentialRangingController,
        sequentialRangingDisplay,
        canvas,
        Serial);

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

#if NT_SHELL_ENABLED
    ntShell.RegisterCommands(preferenceCommands.GetCommands());
    ntShell.Start();
#endif
    if (!rangingDisplayTaskController.Begin())
    {
        rangingDisplayTaskController.ShowTaskStartFailure();
    }
}

/**
 * @brief FreeRTOSの専用タスクへ実行権を渡します。
 */
void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000U));
}
