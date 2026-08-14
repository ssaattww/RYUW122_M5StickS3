#include "SequentialRangingDisplay.h"

/**
 * @brief 逐次測距eventと描画先を注入して表示管理を生成します。
 *
 * @param controller 逐次測距eventの取得元
 * @param broadcast 受信ノード状態の取得元
 * @param canvas 描画先Canvas
 */
SequentialRangingDisplay::SequentialRangingDisplay(
    SequentialRangingController& controller,
    EspNowBroadcast& broadcast,
    M5Canvas& canvas)
    : m_controller(controller),
      m_broadcast(broadcast),
      m_canvas(canvas),
      m_latestResetGeneration(controller.GetResetGeneration())
{
}

/**
 * @brief 通信とUWBの起動結果を永続表示状態へ保存します。
 *
 * @param ryuw122Result RYUW122の初期化結果
 * @param transportStarted ESP-NOW transportを開始できた場合はtrue
 * @param broadcastStarted NodeStatus broadcastを開始できた場合はtrue
 */
void SequentialRangingDisplay::SetInitializationHealth(
    EnRyuw122InitResult ryuw122Result,
    bool transportStarted,
    bool broadcastStarted)
{
    m_ryuw122Result = ryuw122Result;
    m_transportStarted = transportStarted;
    m_broadcastStarted = broadcastStarted;
}

/**
 * @brief 新しい測距結果、ラウンド統計、ノード状態を表示状態へ反映します。
 *
 * @return 再描画が必要な場合はtrue、それ以外はfalse
 */
bool SequentialRangingDisplay::Update()
{
    bool changed = false;

    const uint32_t resetGeneration = m_controller.GetResetGeneration();
    if (resetGeneration != m_latestResetGeneration)
    {
        m_latestResetGeneration = resetGeneration;
        m_latestMeasurement = TimedRangeMeasurement{};
        m_latestSummary = SequentialRangeRoundSummary{};
        m_hasMeasurement = false;
        m_hasSummary = false;
        changed = true;
    }

    TimedRangeMeasurement measurement{};
    while (m_controller.TryTakeMeasurement(measurement))
    {
        m_latestMeasurement = measurement;
        m_hasMeasurement = true;
        changed = true;
    }

    SequentialRangeRoundSummary summary{};
    while (m_controller.TryTakeCompletedRound(summary))
    {
        m_latestSummary = summary;
        m_hasSummary = true;
        changed = true;
    }

    NodeStatus status{};
    while (m_broadcast.TryReceive(status))
    {
        changed = true;
    }

    const EnSequentialRangingState currentState = m_controller.GetState();
    if (currentState != m_latestState)
    {
        m_latestState = currentState;
        changed = true;
    }

    return changed;
}

/**
 * @brief ステータスバーを避けて逐次測距状態と受信ノードを描画します。
 *
 * @param mode 現在の動作モード
 */
void SequentialRangingDisplay::Draw(EnRunMode mode)
{
    m_canvas.fillRect(
        0,
        m_statusBarHeight,
        m_canvas.width(),
        m_canvas.height() - m_statusBarHeight,
        TFT_BLACK);
    m_canvas.setTextColor(TFT_WHITE);
    m_canvas.setTextSize(1);

    if (HasInitializationFailure())
    {
        DrawInitializationFailure();
        return;
    }

    const EnTimeQuality quality = m_hasMeasurement
        ? m_latestMeasurement.timeQuality
        : EnTimeQuality::Unsynchronized;
    m_canvas.setCursor(m_contentLeft, m_firstLineY);
    m_canvas.printf(
        "SEQ %s %s Q:%s",
        GetRoleName(mode, m_latestState),
        GetStateName(m_latestState),
        GetTimeQualityName(quality));

    if (m_hasMeasurement)
    {
        m_canvas.setCursor(m_contentLeft, m_firstLineY + m_lineHeight);
        m_canvas.printf(
            "R%lu A%u-T%u %s",
            static_cast<unsigned long>(m_latestMeasurement.roundId),
            m_latestMeasurement.anchorId,
            m_latestMeasurement.tagId,
            GetResultName(m_latestMeasurement.status));

        m_canvas.setCursor(m_contentLeft, m_firstLineY + (m_lineHeight * 2));
        m_canvas.printf(
            "%lumm RSSI:%d",
            static_cast<unsigned long>(m_latestMeasurement.distanceMm),
            static_cast<int>(m_latestMeasurement.uwbRssi));

        m_canvas.setCursor(m_contentLeft, m_firstLineY + (m_lineHeight * 3));
        m_canvas.printf(
            "dur:%luus Q:%s",
            static_cast<unsigned long>(m_latestMeasurement.rangingDurationUs),
            GetTimeQualityName(m_latestMeasurement.timeQuality));
    }

    if (m_hasSummary)
    {
        const uint8_t missingCount =
            m_latestSummary.expectedMeasurementCount >
                m_latestSummary.receivedMeasurementCount
            ? static_cast<uint8_t>(
                m_latestSummary.expectedMeasurementCount -
                m_latestSummary.receivedMeasurementCount)
            : 0U;

        m_canvas.setCursor(m_contentLeft, m_firstLineY + (m_lineHeight * 4));
        m_canvas.printf(
            "SUM R%lu %u/%u %s",
            static_cast<unsigned long>(m_latestSummary.roundId),
            m_latestSummary.receivedMeasurementCount,
            m_latestSummary.expectedMeasurementCount,
            m_latestSummary.timedOut ? "TIMEOUT" : "DONE");

        m_canvas.setCursor(m_contentLeft, m_firstLineY + (m_lineHeight * 5));
        m_canvas.printf(
            "dur:%luus MISS:%u",
            static_cast<unsigned long>(m_latestSummary.totalDurationUs),
            missingCount);
    }

    DrawReceivedNodes();
}

/**
 * @brief 逐次測距状態の短縮表示名を取得します。
 *
 * @param state 逐次測距状態
 * @return 画面へ表示する状態名
 */
const char* SequentialRangingDisplay::GetStateName(
    EnSequentialRangingState state)
{
    switch (state)
    {
        case EnSequentialRangingState::WaitingForMaster:
            return "WAIT";
        case EnSequentialRangingState::FollowingMaster:
            return "FOLLOW";
        case EnSequentialRangingState::WaitingForSynchronization:
            return "SYNC";
        case EnSequentialRangingState::ReadyToStart:
            return "READY";
        case EnSequentialRangingState::RunningRound:
            return "RUN";
        case EnSequentialRangingState::AnchorIdle:
            return "IDLE";
        case EnSequentialRangingState::AnchorRanging:
            return "RANGE";
        default:
            return "?";
    }
}

/**
 * @brief 動作モードと逐次測距状態から役割名を取得します。
 *
 * @param mode 現在の動作モード
 * @param state 現在の逐次測距状態
 * @return マスター、フォロワー、ANCHORを表す短縮名
 */
const char* SequentialRangingDisplay::GetRoleName(
    EnRunMode mode,
    EnSequentialRangingState state)
{
    if (mode == EnRunMode::Anchor)
    {
        return "A";
    }
    if (state == EnSequentialRangingState::FollowingMaster)
    {
        return "F";
    }
    if (state == EnSequentialRangingState::WaitingForMaster)
    {
        return "?";
    }
    return "M";
}

/**
 * @brief 測距結果状態の短縮表示名を取得します。
 *
 * @param status 測距結果状態
 * @return 画面へ表示する結果名
 */
const char* SequentialRangingDisplay::GetResultName(EnRangeResultStatus status)
{
    switch (status)
    {
        case EnRangeResultStatus::Success:
            return "OK";
        case EnRangeResultStatus::Failed:
            return "FAIL";
        case EnRangeResultStatus::TimedOut:
            return "TIMEOUT";
        case EnRangeResultStatus::Unreachable:
            return "MISS";
        default:
            return "?";
    }
}

/**
 * @brief 時刻品質の短縮表示名を取得します。
 *
 * @param quality 時刻品質
 * @return 画面へ表示する品質名
 */
const char* SequentialRangingDisplay::GetTimeQualityName(EnTimeQuality quality)
{
    switch (quality)
    {
        case EnTimeQuality::Synchronized:
            return "SYNC";
        case EnTimeQuality::PowerSaveEnabled:
            return "PWR";
        case EnTimeQuality::ReceiveTimestampUnavailable:
            return "RX?";
        case EnTimeQuality::SynchronizationExpired:
            return "OLD";
        case EnTimeQuality::Unsynchronized:
            return "UNSYNC";
        default:
            return "?";
    }
}

/**
 * @brief 初期化失敗が保持されているか確認します。
 *
 * @return RYUW122またはESP-NOWの初期化に失敗した場合はtrue
 */
bool SequentialRangingDisplay::HasInitializationFailure() const
{
    return m_ryuw122Result != EnRyuw122InitResult::Ok ||
        !m_transportStarted ||
        !m_broadcastStarted;
}

/**
 * @brief 通常表示より優先して保持中の初期化失敗を描画します。
 */
void SequentialRangingDisplay::DrawInitializationFailure()
{
    int lineY = m_firstLineY;
    m_canvas.setTextColor(TFT_RED);
    m_canvas.setCursor(m_contentLeft, lineY);
    m_canvas.print("INIT FAILED");

    if (m_ryuw122Result != EnRyuw122InitResult::Ok)
    {
        lineY += m_lineHeight;
        m_canvas.setCursor(m_contentLeft, lineY);
        m_canvas.printf(
            "RYUW122: %s",
            Ryuw122Controller::GetResultName(m_ryuw122Result));
    }
    if (!m_transportStarted)
    {
        lineY += m_lineHeight;
        m_canvas.setCursor(m_contentLeft, lineY);
        m_canvas.print("ESP-NOW transport failed");
    }
    else if (!m_broadcastStarted)
    {
        lineY += m_lineHeight;
        m_canvas.setCursor(m_contentLeft, lineY);
        m_canvas.print("ESP-NOW broadcast failed");
    }
}

/**
 * @brief 受信ノード一覧のヘッダーと先頭2件を描画します。
 */
void SequentialRangingDisplay::DrawReceivedNodes()
{
    const int headerY = m_firstLineY + (m_lineHeight * 6);
    if (headerY >= m_canvas.height())
    {
        return;
    }

    m_canvas.setCursor(m_contentLeft, headerY);
    m_canvas.print("ID MODE X,Y");

    int lineY = headerY + m_lineHeight;
    size_t displayedCount = 0;
    for (const auto& node : m_broadcast.GetNodes())
    {
        if (displayedCount >= m_visibleNodeCount || lineY >= m_canvas.height())
        {
            break;
        }

        const NodeStatus& status = node.second;
        const char mode = status.mode == EnRunMode::Tag ? 'T' : 'A';
        m_canvas.setCursor(m_contentLeft, lineY);
        m_canvas.printf(
            "%u %c %u,%u",
            status.nodeID,
            mode,
            status.anchorPositionX,
            status.anchorPositionY);
        lineY += m_lineHeight;
        ++displayedCount;
    }
}
