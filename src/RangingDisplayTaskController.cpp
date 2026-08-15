#include "RangingDisplayTaskController.h"

#include <M5Unified.h>

#include <cstdio>

#include "ConfigPreference.h"
#include "EspNowBroadcast.h"
#include "EspNowReceiveQueueTerminator.h"
#include "EspNowTransport.h"
#include "NtpTimeSynchronizer.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"
#include "SequentialRangingDisplay.h"
#include "TagMasterCoordinator.h"

namespace
{
    constexpr int StatusBarHeight = 20;
}

/**
 * @brief 高優先度更新対象と低優先度描画対象を注入します。
 *
 * @param transport ESP-NOW transport
 * @param receiveQueueTerminator ESP-NOW受信FIFO最終所有者
 * @param broadcast NodeStatus broadcast
 * @param masterCoordinator TAGマスター選出
 * @param timeSynchronizer NTP時刻同期
 * @param ryuw122Controller RYUW122非同期測距
 * @param rangingController 逐次測距controller
 * @param display 表示modelと描画処理
 * @param canvas M5画面へ転送するsprite
 */
RangingDisplayTaskController::RangingDisplayTaskController(
    EspNowTransport& transport,
    EspNowReceiveQueueTerminator& receiveQueueTerminator,
    EspNowBroadcast& broadcast,
    TagMasterCoordinator& masterCoordinator,
    NtpTimeSynchronizer& timeSynchronizer,
    Ryuw122Controller& ryuw122Controller,
    SequentialRangingController& rangingController,
    SequentialRangingDisplay& display,
    M5Canvas& canvas)
    : m_transport(transport),
      m_receiveQueueTerminator(receiveQueueTerminator),
      m_broadcast(broadcast),
      m_masterCoordinator(masterCoordinator),
      m_timeSynchronizer(timeSynchronizer),
      m_ryuw122Controller(ryuw122Controller),
      m_rangingController(rangingController),
      m_display(display),
      m_canvas(canvas)
{
}

/**
 * @brief snapshot queueと高・低優先度タスクを開始します。
 *
 * @return 全queueとタスクを開始できた場合はtrue、それ以外はfalse
 */
bool RangingDisplayTaskController::Begin()
{
    if (m_started)
    {
        return false;
    }

    m_snapshotQueue = xQueueCreate(
        1U,
        sizeof(SequentialRangingDisplaySnapshot));
    if (m_snapshotQueue == nullptr)
    {
        End();
        return false;
    }

    PublishSnapshot();
    if (xTaskCreatePinnedToCore(
            RangingTaskEntry,
            "ranging",
            m_rangingTaskStackSize,
            this,
            m_rangingTaskPriority,
            &m_rangingTask,
            m_rangingTaskCore) != pdPASS)
    {
        End();
        return false;
    }
    if (xTaskCreatePinnedToCore(
            DisplayTaskEntry,
            "display",
            m_displayTaskStackSize,
            this,
            m_displayTaskPriority,
            &m_displayTask,
            m_displayTaskCore) != pdPASS)
    {
        End();
        return false;
    }

    m_started = true;
    return true;
}

/**
 * @brief task開始失敗をCanvasへ描画し、M5画面へ永続転送します。
 */
void RangingDisplayTaskController::ShowTaskStartFailure()
{
    m_canvas.fillSprite(TFT_BLACK);
    m_canvas.setTextColor(TFT_RED);
    m_canvas.setTextSize(1);
    m_canvas.setCursor(4, 23);
    m_canvas.print("TASK START FAILED");
    m_canvas.pushSprite(0, 0);
}

/**
 * @brief 実行中のタスクを停止し、task間queueを解放します。
 */
void RangingDisplayTaskController::End()
{
    if (m_displayTask != nullptr)
    {
        vTaskDelete(m_displayTask);
        m_displayTask = nullptr;
    }
    if (m_rangingTask != nullptr)
    {
        vTaskDelete(m_rangingTask);
        m_rangingTask = nullptr;
    }
    if (m_snapshotQueue != nullptr)
    {
        vQueueDelete(m_snapshotQueue);
        m_snapshotQueue = nullptr;
    }
    m_started = false;
}

/**
 * @brief タスク分離が開始済みか確認します。
 *
 * @return 開始済みの場合はtrue、それ以外はfalse
 */
bool RangingDisplayTaskController::IsStarted() const
{
    return m_started;
}

/**
 * @brief FreeRTOSから高優先度測距タスクを開始します。
 *
 * @param context RangingDisplayTaskControllerインスタンス
 */
void RangingDisplayTaskController::RangingTaskEntry(void* context)
{
    auto* controller = static_cast<RangingDisplayTaskController*>(context);
    for (;;)
    {
        controller->UpdateRangingCycle();
        vTaskDelay(1U);
    }
}

/**
 * @brief FreeRTOSから低優先度画面タスクを開始します。
 *
 * @param context RangingDisplayTaskControllerインスタンス
 */
void RangingDisplayTaskController::DisplayTaskEntry(void* context)
{
    auto* controller = static_cast<RangingDisplayTaskController*>(context);
    controller->RunDisplayTask();
}

/**
 * @brief 通信、同期、UWB、逐次測距、表示modelを1回更新します。
 */
void RangingDisplayTaskController::UpdateRangingCycle()
{
    m_transport.Update();
    m_receiveQueueTerminator.BeginCycle();
    m_broadcast.Update();
    m_masterCoordinator.Update(millis());
    m_timeSynchronizer.Update();
    m_ryuw122Controller.Update();
    m_rangingController.Update();
    m_receiveQueueTerminator.Update();
    if (m_display.Update())
    {
        PublishSnapshot();
    }
}

/**
 * @brief 最新snapshotをtask間queueへ上書き保存します。
 */
void RangingDisplayTaskController::PublishSnapshot()
{
    SequentialRangingDisplaySnapshot snapshot{};
    m_display.CaptureSnapshot(snapshot);
    xQueueOverwrite(m_snapshotQueue, &snapshot);
}

/**
 * @brief 低優先度タスク上でM5入力と画面転送を繰り返します。
 */
void RangingDisplayTaskController::RunDisplayTask()
{
    SequentialRangingDisplaySnapshot snapshot{};
    for (;;)
    {
        M5.update();
        SequentialRangingDisplaySnapshot receivedSnapshot{};
        if (xQueueReceive(
                m_snapshotQueue,
                &receivedSnapshot,
                0U) == pdTRUE)
        {
            snapshot = receivedSnapshot;
            m_canvas.fillSprite(TFT_BLACK);
            DrawStatus(snapshot);
            m_display.Draw(snapshot);
            m_canvas.pushSprite(0, 0);
        }
        vTaskDelay(1U);
    }
}

/**
 * @brief snapshotのノード状態とバッテリー残量を描画します。
 *
 * @param snapshot 描画する固定長snapshot
 */
void RangingDisplayTaskController::DrawStatus(
    const SequentialRangingDisplaySnapshot& snapshot)
{
    m_canvas.fillRect(
        0,
        0,
        m_canvas.width(),
        StatusBarHeight,
        TFT_DARKGREY);
    m_canvas.setTextColor(TFT_WHITE);
    m_canvas.setTextSize(1);

    char statusText[20];
    snprintf(
        statusText,
        sizeof(statusText),
        "ID:%u %s",
        snapshot.m_nodeId,
        ConfigPreference::GetModeName(snapshot.m_mode));
    m_canvas.setCursor(4, 6);
    m_canvas.print(statusText);

    char batteryText[8];
    snprintf(
        batteryText,
        sizeof(batteryText),
        "%d%%",
        M5.Power.getBatteryLevel());
    m_canvas.setCursor(
        m_canvas.width() - m_canvas.textWidth(batteryText) - 4,
        6);
    m_canvas.print(batteryText);
}
