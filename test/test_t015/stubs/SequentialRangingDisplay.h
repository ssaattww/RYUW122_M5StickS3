#pragma once

#include <cstdint>

#include "M5Unified.h"
#include "RunMode.h"
#include "TaskTestRuntime.h"

/**
 * @brief task間queueで渡すtest用表示snapshotを表します。
 */
struct SequentialRangingDisplaySnapshot
{
    EnRunMode m_mode = EnRunMode::Anchor;
    uint8_t m_nodeId = 7U;
    uint32_t m_generation = 0;
};

/**
 * @brief production task controllerへ注入するtest用表示管理を表します。
 */
class SequentialRangingDisplay
{
public:
    /**
     * @brief 表示model更新順を記録します。
     *
     * @return test runtimeで設定した更新有無
     */
    bool Update()
    {
        RecordTaskTestUpdate("display");
        return g_taskTestRuntime.m_displayUpdateChanged;
    }

    /**
     * @brief 世代番号付きsnapshotを生成します。
     *
     * @param snapshot 生成先snapshot
     */
    void CaptureSnapshot(SequentialRangingDisplaySnapshot& snapshot) const
    {
        snapshot = SequentialRangingDisplaySnapshot{};
        snapshot.m_generation = ++g_taskTestRuntime.m_captureGeneration;
        g_taskTestRuntime.m_events.emplace_back("capture_snapshot");
    }

    /**
     * @brief 描画したsnapshot世代を記録します。
     *
     * @param snapshot 描画対象snapshot
     */
    void Draw(const SequentialRangingDisplaySnapshot& snapshot)
    {
        g_taskTestRuntime.m_drawnGeneration = snapshot.m_generation;
        g_taskTestRuntime.m_events.emplace_back("draw_snapshot");
    }
};
