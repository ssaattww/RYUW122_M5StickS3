#include <M5Unified.h>

#include "NtShell.h"

#include "PreferenceCommands.h"
#include "ConfigPreference.h"
#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "EspNowTransport.h"
#include "NvsPreferenceStore.h"
#include "Ryuw122Controller.h"
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
    Ryuw122Controller ryuw122Controller(Serial1, configRuntime);
    M5Canvas canvas(&M5.Display);

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

    /**
     * @brief 受信ノード一覧のヘッダーと全ノード状態をCanvasへ描画します。
     * Modeは画面幅へ収めるためTagをT、AnchorをAで表示します。
     *
     * @param nodes 描画する受信済みノード一覧
     */
    void DrawReceivedNodes(const EspNowBroadcast::NodeMap& nodes)
    {
        constexpr int ContentLeft = 4;
        constexpr int HeaderLineY = 23;
        constexpr int LineHeight = 12;

        canvas.fillRect(
            0,
            StatusBarHeight,
            canvas.width(),
            canvas.height() - StatusBarHeight,
            TFT_BLACK);
        canvas.setTextColor(TFT_WHITE);
        canvas.setTextSize(1);

        canvas.setCursor(ContentLeft, HeaderLineY);
        canvas.print("ID MODE X,Y");

        int lineY = HeaderLineY + LineHeight;
        for (const auto& node : nodes)
        {
            const NodeStatus& status = node.second;
            const char mode = status.mode == EnRunMode::Tag ? 'T' : 'A';
            canvas.setCursor(ContentLeft, lineY);
            canvas.printf(
                "%u %c %u,%u",
                status.nodeID,
                mode,
                status.anchorPositionX,
                status.anchorPositionY);
            lineY += LineHeight;
        }
    }

    /**
     * @brief mailboxから最新のノード状態を取り出しCanvasへ描画します。
     *
     * @return 受信状態を描画した場合はtrue、それ以外はfalse
     */
    bool TryDrawReceivedNodeStatus()
    {
        NodeStatus status{};
        if (!espNowBroadcast.TryReceive(status))
        {
            return false;
        }

        DrawReceivedNodes(espNowBroadcast.GetNodes());
        return true;
    }

};

/**
 * @brief M5Stack、Canvas、NT-Shell、ESP-NOW通信とマスター選出を初期化します。
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
    const bool espNowStarted =
        transportStarted && espNowBroadcast.Begin();
    if (espNowStarted)
    {
        tagMasterCoordinator.Begin(millis());
    }

    canvas.fillSprite(TFT_BLACK);
    DrawStatus(
        configRuntime.GetRunMode(),
        configRuntime.GetCurrentNodeID());
    if (ryuw122Result != EnRyuw122InitResult::Ok)
    {
        canvas.setTextColor(TFT_RED);
        canvas.setTextSize(1);
        canvas.setCursor(4, 30);
        canvas.printf(
            "RYUW122: %s",
            Ryuw122Controller::GetResultName(ryuw122Result));
    }
    else if (!espNowStarted)
    {
        canvas.setTextColor(TFT_RED);
        canvas.setTextSize(1);
        canvas.setCursor(4, 30);
        canvas.print("ESP-NOW init failed");
    }
    canvas.pushSprite(0, 0);

    ntShell.RegisterCommands(preferenceCommands.GetCommands());
    ntShell.Start();

}

/**
 * @brief 入力、ESP-NOW受信、マスター選出を処理して画面を更新します。
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
    espNowBroadcast.Update();
    tagMasterCoordinator.Update(millis());
    espNowBroadcast.Update();

    canvasChanged = TryDrawReceivedNodeStatus() || canvasChanged;

    if (canvasChanged)
    {
        canvas.pushSprite(0, 0);
    }

    ryuw122Controller.Update();

    delay(1);
}
