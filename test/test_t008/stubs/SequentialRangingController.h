#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief test用逐次測距状態を表します。
 */
enum class EnSequentialRangingState : uint8_t
{
    WaitingForMaster,
    FollowingMaster,
    WaitingForSynchronization,
    ReadyToStart,
    RunningRound,
    AnchorIdle,
    AnchorRanging,
};

/**
 * @brief test用測距結果状態を表します。
 */
enum class EnRangeResultStatus : uint8_t
{
    Success,
    Failed,
    TimedOut,
    Unreachable,
};

/**
 * @brief test用時刻品質を表します。
 */
enum class EnTimeQuality : uint8_t
{
    Synchronized,
    PowerSaveEnabled,
    ReceiveTimestampUnavailable,
    SynchronizationExpired,
    Unsynchronized,
};

/**
 * @brief test用逐次測距結果を表します。
 */
struct TimedRangeMeasurement
{
    uint32_t roundId = 0;
    uint8_t anchorId = 0;
    uint8_t tagId = 0;
    EnRangeResultStatus status = EnRangeResultStatus::Failed;
    uint32_t distanceMm = 0;
    int16_t uwbRssi = 0;
    uint64_t rangingCompletedMasterTimeUs = 0;
    uint32_t rangingDurationUs = 0;
    EnTimeQuality timeQuality = EnTimeQuality::Unsynchronized;
};

/**
 * @brief test用ラウンド完了統計を表します。
 */
struct SequentialRangeRoundSummary
{
    uint32_t roundId = 0;
    uint32_t totalDurationUs = 0;
    uint8_t expectedMeasurementCount = 0;
    uint8_t receivedMeasurementCount = 0;
    bool timedOut = false;
};

/**
 * @brief test用逐次event FIFOとreset世代を管理します。
 */
class SequentialRangingController
{
public:
    /**
     * @brief 測距結果FIFOから1件取得します。
     *
     * @param measurement 取得結果格納先
     * @return 取得できた場合はtrue、それ以外はfalse
     */
    bool TryTakeMeasurement(TimedRangeMeasurement& measurement)
    {
        if (m_measurementHead >= m_measurementCount)
        {
            return false;
        }
        measurement = m_measurements[m_measurementHead++];
        return true;
    }

    /**
     * @brief ラウンド完了FIFOから1件取得します。
     *
     * @param summary 取得結果格納先
     * @return 取得できた場合はtrue、それ以外はfalse
     */
    bool TryTakeCompletedRound(SequentialRangeRoundSummary& summary)
    {
        if (m_summaryHead >= m_summaryCount)
        {
            return false;
        }
        summary = m_summaries[m_summaryHead++];
        return true;
    }

    /**
     * @brief 現在の逐次測距状態を取得します。
     *
     * @return 現在の逐次測距状態
     */
    EnSequentialRangingState GetState() const
    {
        return m_state;
    }

    /**
     * @brief 現在のreset世代を取得します。
     *
     * @return reset世代
     */
    uint32_t GetResetGeneration() const
    {
        return m_resetGeneration;
    }

    /**
     * @brief test用測距結果をFIFOへ追加します。
     *
     * @param measurement 追加する測距結果
     */
    void PushMeasurement(const TimedRangeMeasurement& measurement)
    {
        m_measurements[m_measurementCount++] = measurement;
    }

    /**
     * @brief test用ラウンド統計をFIFOへ追加します。
     *
     * @param summary 追加するラウンド統計
     */
    void PushSummary(const SequentialRangeRoundSummary& summary)
    {
        m_summaries[m_summaryCount++] = summary;
    }

    /**
     * @brief test用逐次測距状態を設定します。
     *
     * @param state 設定する状態
     */
    void SetState(EnSequentialRangingState state)
    {
        m_state = state;
    }

    /**
     * @brief master identityまたはsession変更を模擬します。
     */
    void AdvanceResetGeneration()
    {
        ++m_resetGeneration;
    }

    /**
     * @brief 未取得測距結果件数を取得します。
     *
     * @return 未取得測距結果件数
     */
    size_t MeasurementRemaining() const
    {
        return m_measurementCount - m_measurementHead;
    }

    /**
     * @brief 未取得ラウンド統計件数を取得します。
     *
     * @return 未取得ラウンド統計件数
     */
    size_t SummaryRemaining() const
    {
        return m_summaryCount - m_summaryHead;
    }

private:
    TimedRangeMeasurement m_measurements[16]{};
    SequentialRangeRoundSummary m_summaries[8]{};
    size_t m_measurementHead = 0;
    size_t m_measurementCount = 0;
    size_t m_summaryHead = 0;
    size_t m_summaryCount = 0;
    EnSequentialRangingState m_state =
        EnSequentialRangingState::WaitingForMaster;
    uint32_t m_resetGeneration = 0;
};
