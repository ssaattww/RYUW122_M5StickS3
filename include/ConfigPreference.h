#pragma once

#include <Arduino.h>

#include <cstdint>

#include "NvsPreferenceStore.h"

/**
 * @brief 端末の動作モードを表します。
 */
enum class EnRunMode : uint8_t
{
    Tag = 0,
    Anchor = 1,
};

/**
 * @brief アプリケーション設定のキーと初期値をまとめます。
 */
struct ConfigPreferenceDefaults
{
public:
    static constexpr char m_runModeKey[] = "run_mode";
    static constexpr EnRunMode m_defaultRunMode = EnRunMode::Anchor;

    static constexpr char m_espnowChannelKey[] = "espnow_channel";
    static constexpr uint8_t m_defaultEspnowChannel = 4;

    static constexpr char m_wifiPowerSaveKey[] = "wifi_power_save";
    static constexpr bool m_defaultWifiPowerSave = false;

    static constexpr char m_nodeIdKey[] = "node_id";
    static constexpr uint8_t m_defaultNodeId = 0;

    static constexpr char m_anchorPositionXKey[] = "anchor_pos_x";
    static constexpr uint16_t m_defaultAnchorPositionX = 0;

    static constexpr char m_anchorPositionYKey[] = "anchor_pos_y";
    static constexpr uint16_t m_defaultAnchorPositionY = 0;
};

/**
 * @brief アプリケーション固有設定を型情報付きNVSへ保存します。
 */
class ConfigPreference
{
public:
    /**
     * @brief 共通NVSストアを使用する設定アクセサーを生成します。
     *
     * @param store 設定の保存先
     */
    explicit ConfigPreference(NvsPreferenceStore& store);

    /**
     * @brief 現在の動作モードを取得します。
     * 読み出しに失敗した場合はデフォルト値を設定して返します。
     *
     * @param mode 動作モードの格納先
     * @return NVS処理結果
     */
    EnNvsResult GetRunMode(EnRunMode& mode);

    /**
     * @brief 現在の動作モードを保存します。
     *
     * @param mode 保存する動作モード
     * @return NVS処理結果
     */
    EnNvsResult SetCurrentRunMode(EnRunMode mode);

    /**
     * @brief 動作モードの表示名を取得します。
     *
     * @param mode 動作モード
     * @return 動作モードの表示名
     */
    static const char* GetModeName(EnRunMode mode);

    /**
     * @brief 現在のespnowチャンネルを取得します。
     * 
     * @param channel 現在のespnowチャンネルの格納先
     * @return EnNvsResult 
     */
    EnNvsResult GetEspnowChannel(uint8_t& channel);

    /**
     * @brief 現在のespnowチャンネルを保存します。
     * 
     * @param channel 保存するespnowチャンネル
     * @return EnNvsResult 
     */
    EnNvsResult SetCurrentEspnowChannel(uint8_t channel);

    /**
     * @brief Wi-Fi省電力設定を取得します。
     * 読み出しに失敗した場合はデフォルト値を設定して返します。
     *
     * @param wifiPowerSave Wi-Fi省電力設定の格納先
     * @return NVS処理結果
     */
    EnNvsResult GetWifiPowerSave(bool& wifiPowerSave);

    /**
     * @brief Wi-Fi省電力設定を保存します。
     *
     * @param wifiPowerSave 保存するWi-Fi省電力設定
     * @return NVS処理結果
     */
    EnNvsResult SetWifiPowerSave(bool wifiPowerSave);

    /**
     * @brief 現在のノードIDを取得します。
     * 
     * @param nodeID 現在のノードIDの格納先
     * @return EnNvsResult 
     */
    EnNvsResult GetNodeID(uint8_t& nodeID);

    /**
     * @brief 現在のノードIDを保存します。
     * 
     * @param nodeID 保存するノードID
     * @return EnNvsResult 
     */
    EnNvsResult SetNodeID(uint8_t nodeID);

    /**
     * @brief 現在のアンカーX座標を取得します。
     * 
     * @param positionX 
     * @return EnNvsResult 
     */
    EnNvsResult GetAnchorPositionX(uint16_t& positionX);

    /**
     * @brief 現在のアンカーX座標を保存します。
     * 
     * @param positionX 保存するアンカーX座標
     * @return EnNvsResult 
     */
    EnNvsResult SetAnchorPositionX(uint16_t positionX);

    /**
     * @brief 現在のアンカーY座標を取得します。
     * 
     * @param positionY 
     * @return EnNvsResult 
     */
    EnNvsResult GetAnchorPositionY(uint16_t& positionY);

    /**
     * @brief 現在のアンカーY座標を保存します。
     * 
     * @param positionY 保存するアンカーY座標
     * @return EnNvsResult 
     */
    EnNvsResult SetAnchorPositionY(uint16_t positionY);

private:
    NvsPreferenceStore& m_store;
};
