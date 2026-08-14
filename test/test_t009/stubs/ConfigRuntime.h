#pragma once

#include <cstdint>

/**
 * @brief T-009 native統合テスト用の実行時設定を保持します。
 */
class ConfigRuntime
{
public:
    /**
     * @brief ESP-NOWチャンネルを取得します。
     *
     * @return 設定済みチャンネル
     */
    uint8_t GetCurrentEspnowChannel() const
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

private:
    uint8_t m_channel = 6;
    bool m_powerSave = false;
};
