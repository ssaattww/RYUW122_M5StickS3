#pragma once

#include <Arduino.h>

#include "ConfigPreference.h"

class ConfigRuntime
{
private:
    /**
     * @brief 現在の実行モード設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    EnRunMode m_RunMode = ConfigPreferenceDefaults::m_defaultRunMode;
    /**
     * @brief 現在のespnowチャンネル設定値
     * 起動時のデフォルト値変更はNt-Shellコマンドで行うこと。
     */
    uint8_t m_currentEspnowChannel = ConfigPreferenceDefaults::m_defaultEspnowChannel;
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
    ConfigRuntime() = default;
    
    /**
     * @brief 設定値の初期化
     * 
     * @param configPreference 
     */
    void Init(ConfigPreference& configPreference);

    /**
     * @brief Get the Current Run Mode object
     * 
     * @return EnRunMode 
     */
    EnRunMode GetRunMode();

    /**
     * @brief Set the Run Mode object
     * 
     * @param mode 
     */
    void SetRunMode(EnRunMode mode);

    /**
     * @brief Get the Current Espnow Channel object
     * 
     * @return uint8_t 
     */
    uint8_t GetCurrentEspnowChannel();

    /**
     * @brief Set the Current Espnow Channel object
     * 
     * @param channel 
     */
    void SetCurrentEspnowChannel(uint8_t channel);

    /**
     * @brief Get the Current Node ID object
     * 
     * @return uint8_t 
     */
    uint8_t GetCurrentNodeID();

    /**
     * @brief Set the Current Node ID object
     * 
     * @param nodeID 
     */
    void SetCurrentNodeID(uint8_t nodeID);

    /**
     * @brief Get the Anchor Position X object
     * 
     * @return uint16_t 
     */
    uint16_t GetAnchorPositionX();

    /**
     * @brief Set the Anchor Position X object
     * 
     * @param positionX 
     */
    void SetAnchorPositionX(uint16_t positionX);

    /**
     * @brief Get the Anchor Position Y object
     * 
     * @return uint16_t 
     */
    uint16_t GetAnchorPositionY();

    /**
     * @brief Set the Anchor Position Y object
     * 
     * @param positionY 
     */
    void SetAnchorPositionY(uint16_t positionY);
};