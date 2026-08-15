#pragma once

#include <cstdint>
#include <cstring>

#include "Ryuw122Initializer.h"

enum class EnRyuw122RangingStatus : uint8_t
{
    Success,
    Failed,
    TimedOut,
};

struct Ryuw122RangingResult
{
    char tagAddress[9]{};
    EnRyuw122RangingStatus status = EnRyuw122RangingStatus::Failed;
    uint32_t distanceMm = 0;
    int16_t uwbRssi = 0;
    uint32_t startedAtUs = 0;
    uint32_t completedAtUs = 0;
    EnRyuw122RangingReason reason = EnRyuw122RangingReason::ParseError;
    int32_t diagnosticCode = 0;
};

class Ryuw122Controller
{
public:
    void Update()
    {
    }

    bool StartRanging(const char* tagAddress)
    {
        if (!m_startSucceeds || m_startCount >= 32U)
        {
            return false;
        }
        memcpy(m_started[m_startCount++], tagAddress, 9);
        return true;
    }

    bool TryTakeResult(Ryuw122RangingResult& result)
    {
        if (!m_hasResult)
        {
            return false;
        }
        result = m_result;
        m_hasResult = false;
        return true;
    }

    /**
     * @brief test用の結果待ちまたはdrain中状態を返します。
     *
     * @return 開始を保留すべき場合はtrue
     */
    bool IsBusy() const
    {
        return m_forcedBusy || m_hasResult;
    }

    /**
     * @brief test用にdrain相当のBusy状態を設定します。
     *
     * @param busy Busyとして扱う場合はtrue
     */
    void SetBusy(bool busy)
    {
        m_forcedBusy = busy;
    }

    void Complete(const Ryuw122RangingResult& result)
    {
        m_result = result;
        m_hasResult = true;
    }

    const char* StartedAt(size_t index) const
    {
        return m_started[index];
    }

    size_t StartCount() const
    {
        return m_startCount;
    }

    bool m_startSucceeds = true;

private:
    char m_started[32][9]{};
    size_t m_startCount = 0;
    Ryuw122RangingResult m_result{};
    bool m_hasResult = false;
    bool m_forcedBusy = false;
};
