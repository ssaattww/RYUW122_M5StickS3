#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Ryuw122Initializer.h"

/**
 * @brief native統合テストの疑似時刻を取得します。
 *
 * @return 現在の疑似マイクロ秒時刻
 */
uint64_t GetIntegrationTimeUs();

/**
 * @brief T-009 native統合テスト用の測距完了状態を表します。
 */
enum class EnRyuw122RangingStatus : uint8_t
{
    Success,
    Failed,
    TimedOut,
};

/**
 * @brief T-009 native統合テスト用の測距結果を表します。
 */
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

/**
 * @brief production測距状態機械へ非同期成功結果を返すfake UWBです。
 */
class Ryuw122Controller
{
public:
    /**
     * @brief 受付済み測距を次の取得で返せる状態へ進めます。
     */
    void Update()
    {
        if (!m_autoComplete || !m_requestPending || m_resultReady)
        {
            return;
        }
        memcpy(m_result.tagAddress, m_pendingTagAddress, 9);
        m_result.status = EnRyuw122RangingStatus::Success;
        m_result.distanceMm = 1000U + static_cast<uint32_t>(m_startCount);
        m_result.uwbRssi = -70;
        m_result.startedAtUs = m_startedAtUs;
        m_result.completedAtUs = static_cast<uint32_t>(GetIntegrationTimeUs());
        m_result.reason = EnRyuw122RangingReason::Success;
        m_requestPending = false;
        m_resultReady = true;
    }

    /**
     * @brief 指定TAGへの測距要求を受け付けます。
     *
     * @param tagAddress 対象TAGの8文字UWBアドレス
     * @return 要求を受け付けた場合はtrue
     */
    bool StartRanging(const char* tagAddress)
    {
        if (tagAddress == nullptr || m_requestPending || m_resultReady ||
            m_startCount >= 16U)
        {
            return false;
        }
        memcpy(m_pendingTagAddress, tagAddress, 9);
        memcpy(m_startedAddresses[m_startCount], tagAddress, 9);
        ++m_startCount;
        m_startedAtUs = static_cast<uint32_t>(GetIntegrationTimeUs());
        m_requestPending = true;
        return true;
    }

    /**
     * @brief 完了済み測距結果を1件取得します。
     *
     * @param result 取得した結果格納先
     * @return 結果が存在する場合はtrue
     */
    bool TryTakeResult(Ryuw122RangingResult& result)
    {
        if (!m_resultReady)
        {
            return false;
        }
        result = m_result;
        m_resultReady = false;
        return true;
    }

    /**
     * @brief 疑似UWBが新しい測距を受け付けられないか確認します。
     *
     * @return requestまたは結果が残っている場合はtrue
     */
    bool IsBusy() const
    {
        return m_requestPending || m_resultReady;
    }

    /**
     * @brief 測距開始回数を取得します。
     *
     * @return 測距開始回数
     */
    size_t GetStartCount() const
    {
        return m_startCount;
    }

    /**
     * @brief 指定indexで開始したTAGアドレスを取得します。
     *
     * @param index 測距開始順index
     * @return 記録したTAGアドレス
     */
    const char* GetStartedAddress(size_t index) const
    {
        return m_startedAddresses[index];
    }

    /**
     * @brief 測距要求の自動完了を切り替えます。
     *
     * @param enabled 自動完了する場合はtrue
     */
    void SetAutoComplete(bool enabled)
    {
        m_autoComplete = enabled;
    }

private:
    char m_pendingTagAddress[9]{};
    char m_startedAddresses[16][9]{};
    size_t m_startCount = 0;
    uint32_t m_startedAtUs = 0;
    Ryuw122RangingResult m_result{};
    bool m_autoComplete = true;
    bool m_requestPending = false;
    bool m_resultReady = false;
};
