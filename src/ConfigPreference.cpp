#include "ConfigPreference.h"

constexpr char ConfigPreferenceDefaults::m_runModeKey[];
constexpr EnRunMode ConfigPreferenceDefaults::m_defaultRunMode;
constexpr char ConfigPreferenceDefaults::m_espnowChannelKey[];
constexpr uint8_t ConfigPreferenceDefaults::m_defaultEspnowChannel;
constexpr char ConfigPreferenceDefaults::m_nodeIdKey[];
constexpr uint8_t ConfigPreferenceDefaults::m_defaultNodeId;
constexpr char ConfigPreferenceDefaults::m_anchorPositionXKey[];
constexpr uint16_t ConfigPreferenceDefaults::m_defaultAnchorPositionX;
constexpr char ConfigPreferenceDefaults::m_anchorPositionYKey[];
constexpr uint16_t ConfigPreferenceDefaults::m_defaultAnchorPositionY;

ConfigPreference::ConfigPreference(NvsPreferenceStore& store)
    : m_store(store)
{
}

EnNvsResult ConfigPreference::GetRunMode(EnRunMode& mode)
{
    uint8_t storedMode = static_cast<uint8_t>(
        ConfigPreferenceDefaults::m_defaultRunMode);
    const EnNvsResult result = m_store.GetU8(
        ConfigPreferenceDefaults::m_runModeKey,
        storedMode);
    if (result == EnNvsResult::NotFound)
    {
        return SetCurrentRunMode(ConfigPreferenceDefaults::m_defaultRunMode);
    }
    else if (result != EnNvsResult::Ok)
    {
        return result;
    }

    switch (storedMode)
    {
    case static_cast<uint8_t>(EnRunMode::Tag):
        mode = EnRunMode::Tag;
        return EnNvsResult::Ok;
    case static_cast<uint8_t>(EnRunMode::Anchor):
        mode = EnRunMode::Anchor;
        return EnNvsResult::Ok;
    default:
        return EnNvsResult::InvalidValue;
    }
}

EnNvsResult ConfigPreference::SetCurrentRunMode(EnRunMode mode)
{
    switch (mode)
    {
    case EnRunMode::Tag:
    case EnRunMode::Anchor:
        return m_store.SetU8(
            ConfigPreferenceDefaults::m_runModeKey,
            static_cast<uint8_t>(mode));
    default:
        return EnNvsResult::InvalidValue;
    }
}

const char* ConfigPreference::GetModeName(EnRunMode mode)
{
    switch (mode)
    {
    case EnRunMode::Tag:
        return "TAG";

    case EnRunMode::Anchor:
        return "ANCHOR";
    }

    return "UNKNOWN";
}

EnNvsResult ConfigPreference::GetEspnowChannel(uint8_t& channel)
{
    channel = ConfigPreferenceDefaults::m_defaultEspnowChannel;
    const EnNvsResult result = m_store.GetU8(
        ConfigPreferenceDefaults::m_espnowChannelKey,
        channel);
    if (result == EnNvsResult::NotFound)
    {
        return SetCurrentEspnowChannel(
            ConfigPreferenceDefaults::m_defaultEspnowChannel);
    }
    else if (result != EnNvsResult::Ok)
    {
        return result;
    }

    return result;
}

EnNvsResult ConfigPreference::SetCurrentEspnowChannel(uint8_t channel)
{
    return m_store.SetU8(
        ConfigPreferenceDefaults::m_espnowChannelKey,
        channel);
}

EnNvsResult ConfigPreference::GetNodeID(uint8_t& nodeID)
{
    nodeID = ConfigPreferenceDefaults::m_defaultNodeId;
    const EnNvsResult result = m_store.GetU8(
        ConfigPreferenceDefaults::m_nodeIdKey,
        nodeID);
    if (result == EnNvsResult::NotFound)
    {
        return SetNodeID(ConfigPreferenceDefaults::m_defaultNodeId);
    }
    else if (result != EnNvsResult::Ok)
    {
        return result;
    }

    return result;
}

EnNvsResult ConfigPreference::SetNodeID(uint8_t nodeID)
{
    return m_store.SetU8(ConfigPreferenceDefaults::m_nodeIdKey, nodeID);
}

EnNvsResult ConfigPreference::GetAnchorPositionX(uint16_t& positionX)
{
    positionX = ConfigPreferenceDefaults::m_defaultAnchorPositionX;
    const EnNvsResult result = m_store.GetU16(
        ConfigPreferenceDefaults::m_anchorPositionXKey,
        positionX);
    if (result == EnNvsResult::NotFound)
    {
        return SetAnchorPositionX(ConfigPreferenceDefaults::m_defaultAnchorPositionX);
    }
    else if (result != EnNvsResult::Ok)
    {
        return result;
    }

    return result;
}

EnNvsResult ConfigPreference::SetAnchorPositionX(uint16_t positionX)
{
    return m_store.SetU16(
        ConfigPreferenceDefaults::m_anchorPositionXKey,
        positionX);
}

EnNvsResult ConfigPreference::GetAnchorPositionY(uint16_t& positionY)
{
    positionY = ConfigPreferenceDefaults::m_defaultAnchorPositionY;
    const EnNvsResult result = m_store.GetU16(
        ConfigPreferenceDefaults::m_anchorPositionYKey,
        positionY);
    if (result == EnNvsResult::NotFound)
    {
        return SetAnchorPositionY(ConfigPreferenceDefaults::m_defaultAnchorPositionY);
    }
    else if (result != EnNvsResult::Ok)
    {
        return result;
    }

    return result;
}

EnNvsResult ConfigPreference::SetAnchorPositionY(uint16_t positionY)
{
    return m_store.SetU16(
        ConfigPreferenceDefaults::m_anchorPositionYKey,
        positionY);
}