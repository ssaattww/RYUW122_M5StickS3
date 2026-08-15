#pragma once

#include <cstring>
#include <deque>

#include "Ryuw122Initializer.h"
#include "TaskTestRuntime.h"

/**
 * @brief production task controllerへ渡すtest用測距診断eventです。
 */
struct RangingDiagnosticEvent
{
    char tagAddress[9] = {};
    EnRyuw122RangingReason reason = EnRyuw122RangingReason::ParseError;
    uint32_t distanceMm = 0;
    uint32_t durationMs = 0;
    int32_t diagnosticCode = 0;
    uint8_t anchorId = 0;
    uint8_t tagId = 0;
};

/**
 * @brief test用逐次測距controllerを表します。
 */
class SequentialRangingController
{
public:
    /** @brief 逐次測距更新順を記録します。 */
    void Update()
    {
        RecordTaskTestUpdate("ranging");
    }

    /**
     * @brief test用診断eventをproduction task controllerへ渡します。
     *
     * @param event 診断eventの格納先
     * @return eventが存在する場合はtrue
     */
    bool TryTakeDiagnostic(RangingDiagnosticEvent& event)
    {
        if (m_diagnostics.empty())
        {
            return false;
        }
        event = m_diagnostics.front();
        m_diagnostics.pop_front();
        return true;
    }

    /**
     * @brief testで高優先度側へ供給する診断eventを追加します。
     *
     * @param event 追加する診断event
     */
    void PushDiagnostic(const RangingDiagnosticEvent& event)
    {
        m_diagnostics.push_back(event);
    }

private:
    std::deque<RangingDiagnosticEvent> m_diagnostics;
};
