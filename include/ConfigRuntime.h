#pragma once

#include <Arduino.h>

#include "ConfigPreference.h"

/**
 * @brief 起動時に読み出した設定と実行中の変更値を保持します。
 */
class ConfigRuntime
{
private:
    /**
     * @brief 現在の実行モード設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    EnRunMode m_runMode = ConfigPreferenceDefaults::m_defaultRunMode;
    /**
     * @brief 現在のespnowチャンネル設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    uint8_t m_currentEspnowChannel = ConfigPreferenceDefaults::m_defaultEspnowChannel;
    /**
     * @brief 現在のWi-Fi省電力設定値
     */
    bool m_wifiPowerSave = ConfigPreferenceDefaults::m_defaultWifiPowerSave;
    /**
     * @brief 現在のノードID設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    uint8_t m_currentNodeID = ConfigPreferenceDefaults::m_defaultNodeId;

    /**
     * @brief 現在のアンカーX座標設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    uint16_t m_anchorPositionX = ConfigPreferenceDefaults::m_defaultAnchorPositionX;

    /**
     * @brief 現在のアンカーY座標設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    uint16_t m_anchorPositionY = ConfigPreferenceDefaults::m_defaultAnchorPositionY;

public:
    /**
     * @brief 既定値を保持する実行時設定を生成します。
     */
    ConfigRuntime() = default;
    
    /**
     * @brief NVS設定アクセサーから実行時設定を初期化します。
     *
     * @param configPreference 読み出しに使用する設定アクセサー
     */
    void Init(ConfigPreference& configPreference);

    /**
     * @brief 現在の動作モードを取得します。
     *
     * @return 現在の動作モード
     */
    EnRunMode GetRunMode();

    /**
     * @brief 現在の動作モードを更新します。
     *
     * @param mode 更新する動作モード
     */
    void SetRunMode(EnRunMode mode);

    /**
     * @brief 現在のESP-NOWチャンネルを取得します。
     *
     * @return 現在のESP-NOWチャンネル
     */
    uint8_t GetCurrentEspnowChannel();

    /**
     * @brief 現在のWi-Fi省電力設定を取得します。
     *
     * @return Wi-Fi省電力が有効な場合はtrue、それ以外はfalse
     */
    bool GetWifiPowerSave() const;

    /**
     * @brief 現在のESP-NOWチャンネルを更新します。
     *
     * @param channel 更新するESP-NOWチャンネル
     */
    void SetCurrentEspnowChannel(uint8_t channel);

    /**
     * @brief 現在のノードIDを取得します。
     *
     * @return 現在のノードID
     */
    uint8_t GetCurrentNodeID();

    /**
     * @brief 現在のノードIDを更新します。
     *
     * @param nodeID 更新するノードID
     */
    void SetCurrentNodeID(uint8_t nodeID);

    /**
     * @brief 現在のANCHOR X座標を取得します。
     *
     * @return 現在のANCHOR X座標
     */
    uint16_t GetAnchorPositionX();

    /**
     * @brief 現在のANCHOR X座標を更新します。
     *
     * @param positionX 更新するANCHOR X座標
     */
    void SetAnchorPositionX(uint16_t positionX);

    /**
     * @brief 現在のANCHOR Y座標を取得します。
     *
     * @return 現在のANCHOR Y座標
     */
    uint16_t GetAnchorPositionY();

    /**
     * @brief 現在のANCHOR Y座標を更新します。
     *
     * @param positionY 更新するANCHOR Y座標
     */
    void SetAnchorPositionY(uint16_t positionY);
};
