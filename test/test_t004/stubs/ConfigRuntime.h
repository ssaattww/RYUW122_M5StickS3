#pragma once

#include <cstdint>

/**
 * @brief NTP native test用の実行時設定を保持します。
 */
class ConfigRuntime
{
public:
    /**
     * @brief ESP-NOWチャンネルを取得します。
     *
     * @return 設定済みチャンネル
     */
    uint8_t GetCurrentEspnowChannel()
    {
        return m_channel;
    }

    /**
     * @brief Wi-Fi省電力設定を取得します。
     *
     * @return 省電力が有効な場合はtrue
     */
    bool GetWifiPowerSave() const
    {
        return m_powerSave;
    }

    /**
     * @brief ESP-NOWチャンネルを設定します。
     *
     * @param channel 設定するチャンネル
     */
    void SetChannel(uint8_t channel)
    {
        m_channel = channel;
    }

    /**
     * @brief Wi-Fi省電力設定を更新します。
     *
     * @param enabled 有効にする場合はtrue
     */
    void SetPowerSave(bool enabled)
    {
        m_powerSave = enabled;
    }

private:
    uint8_t m_channel = 1;
    bool m_powerSave = false;
};
