#pragma once

#include <cstdint>

/**
 * @brief test用の動作モードを表します。
 */
enum class EnRunMode : uint8_t
{
    Tag,
    Anchor,
};

/**
 * @brief Ryuw122Controller testに必要な実行時設定を保持します。
 */
class ConfigRuntime
{
public:
    /**
     * @brief 現在の動作モードを取得します。
     *
     * @return 現在の動作モード
     */
    EnRunMode GetRunMode()
    {
        return m_runMode;
    }

    /**
     * @brief 現在の動作モードを設定します。
     *
     * @param mode 設定する動作モード
     */
    void SetRunMode(EnRunMode mode)
    {
        m_runMode = mode;
    }

    /**
     * @brief 現在のノードIDを取得します。
     *
     * @return 現在のノードID
     */
    uint8_t GetCurrentNodeID()
    {
        return m_nodeId;
    }

    /**
     * @brief 現在のノードIDを設定します。
     *
     * @param nodeId 設定するノードID
     */
    void SetCurrentNodeID(uint8_t nodeId)
    {
        m_nodeId = nodeId;
    }

private:
    EnRunMode m_runMode = EnRunMode::Anchor;
    uint8_t m_nodeId = 1;
};
