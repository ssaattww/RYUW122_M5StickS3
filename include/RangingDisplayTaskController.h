#pragma once

#include <M5Unified.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstdint>

#include "BuildOptions.h"

class EspNowBroadcast;
class EspNowReceiveQueueTerminator;
class EspNowTransport;
class NtpTimeSynchronizer;
class Ryuw122Controller;
class SequentialRangingController;
class SequentialRangingDisplay;
class TagMasterCoordinator;
class Print;
struct SequentialRangingDisplaySnapshot;

/**
 * @brief 測距更新とM5画面描画を優先度の異なるFreeRTOSタスクへ分離します。
 */
class RangingDisplayTaskController
{
public:
    static constexpr UBaseType_t m_rangingTaskPriority = 4U;
    static constexpr UBaseType_t m_displayTaskPriority = 1U;
    static constexpr BaseType_t m_rangingTaskCore = 1;
    static constexpr BaseType_t m_displayTaskCore = 0;
    static constexpr uint32_t m_rangingTaskStackSize = 8192U;
    static constexpr uint32_t m_displayTaskStackSize = 6144U;
    static constexpr UBaseType_t m_diagnosticQueueCapacity = 8U;

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
     * @param diagnosticOutput 低優先度タスクだけが使用する診断出力先
     */
    RangingDisplayTaskController(
        EspNowTransport& transport,
        EspNowReceiveQueueTerminator& receiveQueueTerminator,
        EspNowBroadcast& broadcast,
        TagMasterCoordinator& masterCoordinator,
        NtpTimeSynchronizer& timeSynchronizer,
        Ryuw122Controller& ryuw122Controller,
        SequentialRangingController& rangingController,
        SequentialRangingDisplay& display,
        M5Canvas& canvas,
        Print& diagnosticOutput);

    /**
     * @brief snapshot queueと高・低優先度タスクを開始します。
     *
     * @return 全queueとタスクを開始できた場合はtrue、それ以外はfalse
     */
    bool Begin();

    /**
     * @brief task開始失敗をCanvasへ描画し、M5画面へ永続転送します。
     */
    void ShowTaskStartFailure();

    /**
     * @brief 実行中のタスクを停止し、task間queueを解放します。
     */
    void End();

    /**
     * @brief タスク分離が開始済みか確認します。
     *
     * @return 開始済みの場合はtrue、それ以外はfalse
     */
    bool IsStarted() const;

private:
    /**
     * @brief FreeRTOSから高優先度測距タスクを開始します。
     *
     * @param context RangingDisplayTaskControllerインスタンス
     */
    static void RangingTaskEntry(void* context);

    /**
     * @brief FreeRTOSから低優先度画面タスクを開始します。
     *
     * @param context RangingDisplayTaskControllerインスタンス
     */
    static void DisplayTaskEntry(void* context);

    /**
     * @brief 通信、同期、UWB、逐次測距、表示modelを1回更新します。
     */
    void UpdateRangingCycle();

    /**
     * @brief 最新snapshotをtask間queueへ上書き保存します。
     */
    void PublishSnapshot();

    /**
     * @brief ANCHOR測距診断eventを低優先度タスク向けFIFOへ転送します。
     */
    void PublishDiagnostics();

    /**
     * @brief 低優先度タスク上でM5入力と画面転送を繰り返します。
     */
    void RunDisplayTask();

    /**
     * @brief 低優先度タスクで測距診断eventを1行ずつ出力します。
     */
    void FlushDiagnostics();

    /**
     * @brief snapshotのノード状態とバッテリー残量を描画します。
     *
     * @param snapshot 描画する固定長snapshot
     */
    void DrawStatus(const SequentialRangingDisplaySnapshot& snapshot);

    EspNowTransport& m_transport;
    EspNowReceiveQueueTerminator& m_receiveQueueTerminator;
    EspNowBroadcast& m_broadcast;
    TagMasterCoordinator& m_masterCoordinator;
    NtpTimeSynchronizer& m_timeSynchronizer;
    Ryuw122Controller& m_ryuw122Controller;
    SequentialRangingController& m_rangingController;
    SequentialRangingDisplay& m_display;
    M5Canvas& m_canvas;
    Print& m_diagnosticOutput;
    QueueHandle_t m_snapshotQueue = nullptr;
    QueueHandle_t m_diagnosticQueue = nullptr;
    TaskHandle_t m_rangingTask = nullptr;
    TaskHandle_t m_displayTask = nullptr;
    bool m_started = false;
};
