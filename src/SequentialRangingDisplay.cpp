#include "SequentialRangingDisplay.h"

#include <cstdio>

/**
 * @brief 逐次測距eventと描画先を注入して表示管理を生成します。
 *
 * @param controller 逐次測距eventの取得元
 * @param broadcast 受信ノード状態の取得元
 * @param timeSynchronizer 現在のマスターTAG基準時刻の取得元
 * @param canvas 描画先Canvas
 */
SequentialRangingDisplay::SequentialRangingDisplay(
    SequentialRangingController& controller,
    EspNowBroadcast& broadcast,
    NtpTimeSynchronizer& timeSynchronizer,
    M5Canvas& canvas)
    : m_controller(controller),
      m_broadcast(broadcast),
      m_timeSynchronizer(timeSynchronizer),
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
        ClearTagMeasurements();
        changed = true;
    }

    TimedRangeMeasurement measurement{};
    while (m_controller.TryTakeMeasurement(measurement))
    {
        changed = StoreTagMeasurement(measurement) || changed;
    }

    SequentialRangeRoundSummary summary{};
    while (m_controller.TryTakeCompletedRound(summary))
    {
        static_cast<void>(summary);
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

    changed = UpdateCurrentMasterTime() || changed;

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

    const EnTimeQuality quality = mode == EnRunMode::Tag &&
        m_anchorMeasurementCount > 0U
        ? m_latestTimeQuality
        : EnTimeQuality::Unsynchronized;
    m_canvas.setCursor(m_contentLeft, m_firstLineY);
    m_canvas.printf(
        "SEQ %s %s Q:%s",
        GetRoleName(mode, m_latestState),
        GetStateName(m_latestState),
        GetTimeQualityName(quality));

    if (mode == EnRunMode::Tag)
    {
        DrawTagResults();
    }

    DrawReceivedNodes();
}

/**
 * @brief 自TAGに対するANCHOR別最新測距結果と現在のマスター時刻を描画します。
 */
void SequentialRangingDisplay::DrawTagResults()
{
    m_canvas.setCursor(m_contentLeft, m_firstLineY + m_lineHeight);
    if (m_hasCurrentMasterTime)
    {
        m_canvas.printf(
            "NOW %06llus",
            static_cast<unsigned long long>(
                (m_currentMasterTimeUs / 1000000U) %
                m_masterTimeModuloSeconds));
    }
    else
    {
        m_canvas.print("NOW UNSYNC");
    }

    m_canvas.setTextSize(m_tagResultTextScaleX, 1.0F);
    for (size_t index = 0; index < m_anchorMeasurementCount; ++index)
    {
        const TimedRangeMeasurement& measurement =
            m_anchorMeasurements[index];
        const int lineY = m_firstLineY +
            (m_lineHeight *
             (m_tagResultFirstLineIndex + static_cast<int>(index)));
        m_canvas.setCursor(m_contentLeft, lineY);
        char resultText[12]{};
        if (measurement.status == EnRangeResultStatus::Success)
        {
            FormatDistance(
                measurement.distanceMm,
                resultText,
                sizeof(resultText));
        }
        else
        {
            snprintf(
                resultText,
                sizeof(resultText),
                "%s",
                GetResultName(measurement.status));
        }
        if (HasValidMeasurementMasterTime(measurement))
        {
            const unsigned long long measuredSecond =
                static_cast<unsigned long long>(
                    (measurement.rangingCompletedMasterTimeUs / 1000000U) %
                    m_masterTimeModuloSeconds);
            m_canvas.printf(
                "A%u %s@%06llus",
                measurement.anchorId,
                resultText,
                measuredSecond);
        }
        else
        {
            m_canvas.printf(
                "A%u %s@UNSYNC",
                measurement.anchorId,
                resultText);
        }
    }
}

/**
 * @brief 自TAG向け測距結果をANCHOR ID昇順の固定長一覧へ保存します。
 *
 * @param measurement 保存候補の測距結果
 * @return 一覧を更新した場合はtrue、それ以外はfalse
 */
bool SequentialRangingDisplay::StoreTagMeasurement(
    const TimedRangeMeasurement& measurement)
{
    const NodeStatus& localStatus = m_broadcast.GetLocalStatus();
    if (localStatus.mode != EnRunMode::Tag ||
        measurement.tagId != localStatus.nodeID)
    {
        return false;
    }

    size_t insertionIndex = 0;
    while (insertionIndex < m_anchorMeasurementCount &&
        m_anchorMeasurements[insertionIndex].anchorId < measurement.anchorId)
    {
        ++insertionIndex;
    }
    if (insertionIndex < m_anchorMeasurementCount &&
        m_anchorMeasurements[insertionIndex].anchorId == measurement.anchorId)
    {
        m_anchorMeasurements[insertionIndex] = measurement;
        m_latestTimeQuality = measurement.timeQuality;
        return true;
    }
    if (m_anchorMeasurementCount >= m_maxAnchorResultCount)
    {
        return false;
    }

    for (size_t index = m_anchorMeasurementCount;
         index > insertionIndex;
         --index)
    {
        m_anchorMeasurements[index] = m_anchorMeasurements[index - 1U];
    }
    m_anchorMeasurements[insertionIndex] = measurement;
    ++m_anchorMeasurementCount;
    m_latestTimeQuality = measurement.timeQuality;
    return true;
}

/**
 * @brief 保持中のANCHOR別測距結果と表示品質を破棄します。
 */
void SequentialRangingDisplay::ClearTagMeasurements()
{
    for (TimedRangeMeasurement& measurement : m_anchorMeasurements)
    {
        measurement = TimedRangeMeasurement{};
    }
    m_anchorMeasurementCount = 0;
    m_latestTimeQuality = EnTimeQuality::Unsynchronized;
}

/**
 * @brief 現在のマスターTAG基準秒を表示状態へ反映します。
 *
 * @return 表示する秒または有効状態が変化した場合はtrue、それ以外はfalse
 */
bool SequentialRangingDisplay::UpdateCurrentMasterTime()
{
    uint64_t currentMasterTimeUs = 0;
    const bool hasCurrentMasterTime =
        m_broadcast.GetLocalStatus().mode == EnRunMode::Tag &&
        m_timeSynchronizer.TryGetCurrentMasterTime(currentMasterTimeUs);
    if (!hasCurrentMasterTime)
    {
        const bool changed = m_hasCurrentMasterTime;
        m_hasCurrentMasterTime = false;
        m_currentMasterTimeUs = 0;
        return changed;
    }

    const bool changed = !m_hasCurrentMasterTime ||
        currentMasterTimeUs / 1000000U !=
            m_currentMasterTimeUs / 1000000U;
    m_hasCurrentMasterTime = true;
    m_currentMasterTimeUs = currentMasterTimeUs;
    return changed;
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
 * @brief 距離を画面幅へ収まる単位へ変換します。
 *
 * @param distanceMm ミリメートル単位の距離
 * @param text 変換後文字列の格納先
 * @param textSize 格納先のバイト数
 */
void SequentialRangingDisplay::FormatDistance(
    uint32_t distanceMm,
    char* text,
    size_t textSize)
{
    if (distanceMm < 100000U)
    {
        snprintf(
            text,
            textSize,
            "%lumm",
            static_cast<unsigned long>(distanceMm));
        return;
    }
    if (distanceMm < 100000000U)
    {
        snprintf(
            text,
            textSize,
            "%lum",
            static_cast<unsigned long>(distanceMm / 1000U));
        return;
    }
    snprintf(
        text,
        textSize,
        "%lukm",
        static_cast<unsigned long>(distanceMm / 1000000U));
}

/**
 * @brief 測距結果が有効なマスターTAG基準計測時刻を持つか確認します。
 *
 * @param measurement 確認する測距結果
 * @return 時刻変換済みの品質である場合はtrue、それ以外はfalse
 */
bool SequentialRangingDisplay::HasValidMeasurementMasterTime(
    const TimedRangeMeasurement& measurement)
{
    switch (measurement.timeQuality)
    {
        case EnTimeQuality::Synchronized:
        case EnTimeQuality::PowerSaveEnabled:
        case EnTimeQuality::ReceiveTimestampUnavailable:
            return true;
        case EnTimeQuality::SynchronizationExpired:
        case EnTimeQuality::Unsynchronized:
        default:
            return false;
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
 * @brief 受信ノード一覧のヘッダーと先頭3件を描画します。
 */
void SequentialRangingDisplay::DrawReceivedNodes()
{
    m_canvas.setTextSize(1);
    const int headerY = m_firstLineY +
        (m_lineHeight * m_receivedNodeHeaderLineIndex);
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
