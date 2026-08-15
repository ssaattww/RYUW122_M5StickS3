#pragma once

#include <cstdint>

/**
 * @brief test用の現在マスターTAG基準時刻を提供します。
 */
class NtpTimeSynchronizer
{
public:
    /**
     * @brief 現在のマスターTAG基準時刻を取得します。
     *
     * @param masterTimeUs 現在時刻格納先
     * @return 時刻が有効な場合はtrue
     */
    bool TryGetCurrentMasterTime(uint64_t& masterTimeUs) const
    {
        if (!m_isValid)
        {
            return false;
        }
        masterTimeUs = m_masterTimeUs;
        return true;
    }

    /**
     * @brief test用の現在マスターTAG基準時刻を設定します。
     *
     * @param masterTimeUs 設定する現在時刻
     */
    void SetCurrentMasterTime(uint64_t masterTimeUs)
    {
        m_masterTimeUs = masterTimeUs;
        m_isValid = true;
    }

    /**
     * @brief 現在マスターTAG基準時刻を無効化します。
     */
    void ClearCurrentMasterTime()
    {
        m_masterTimeUs = 0;
        m_isValid = false;
    }

private:
    uint64_t m_masterTimeUs = 0;
    bool m_isValid = false;
};
