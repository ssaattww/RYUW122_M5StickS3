#include <M5Unified.h>

#include "NtShell.h"

#include "PreferenceCommands.h"
#include "ConfigPreference.h"
#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "NvsPreferenceStore.h"

namespace
{
    NtShell ntShell(Serial);
    NvsPreferenceStore preferenceStore("ryuw122", "ryuw122_meta");
    PreferenceCommands preferenceCommands(preferenceStore);
    ConfigPreference configPreference(preferenceStore);
    ConfigRuntime configRuntime;
    EspNowBroadcast espNowBroadcast(configRuntime);
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
 * @brief M5Stack、Canvas、NT-Shell、ESP-NOWブロードキャストを初期化します。
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

    const bool espNowStarted = espNowBroadcast.Begin();

    canvas.fillSprite(TFT_BLACK);
    DrawStatus(
        configRuntime.GetRunMode(),
        configRuntime.GetCurrentNodeID());
    if (!espNowStarted)
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
 * @brief M5Stackの入力とESP-NOW受信を処理し、変更したCanvasを実画面へ転送します。
 */
void loop()
{
    M5.update();
    bool canvasChanged = TryDrawReceivedNodeStatus();
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

    if (canvasChanged)
    {
        canvas.pushSprite(0, 0);
    }

    espNowBroadcast.Update();

    delay(1);
}
