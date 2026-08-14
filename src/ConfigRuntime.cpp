#include "ConfigRuntime.h"

void ConfigRuntime::Init(ConfigPreference& configPreference)
{
    if (configPreference.GetRunMode(m_runMode) != EnNvsResult::Ok)
    {
        m_runMode = ConfigPreferenceDefaults::m_defaultRunMode;
    }

    if (configPreference.GetEspnowChannel(m_currentEspnowChannel) != EnNvsResult::Ok)
    {
        m_currentEspnowChannel = ConfigPreferenceDefaults::m_defaultEspnowChannel;
    }

    if (configPreference.GetWifiPowerSave(m_wifiPowerSave) != EnNvsResult::Ok)
    {
        m_wifiPowerSave = ConfigPreferenceDefaults::m_defaultWifiPowerSave;
    }

    if (configPreference.GetNodeID(m_currentNodeID) != EnNvsResult::Ok)
    {
        m_currentNodeID = ConfigPreferenceDefaults::m_defaultNodeId;
    }

    if (configPreference.GetAnchorPositionX(m_anchorPositionX) != EnNvsResult::Ok)
    {
        m_anchorPositionX = ConfigPreferenceDefaults::m_defaultAnchorPositionX;
    }

    if (configPreference.GetAnchorPositionY(m_anchorPositionY) != EnNvsResult::Ok)
    {
        m_anchorPositionY = ConfigPreferenceDefaults::m_defaultAnchorPositionY;
    }
}

EnRunMode ConfigRuntime::GetRunMode()
{
    return m_runMode;
}

void ConfigRuntime::SetRunMode(EnRunMode mode)
{
    m_runMode = mode;
}

uint8_t ConfigRuntime::GetCurrentEspnowChannel()
{
    return m_currentEspnowChannel;
}

bool ConfigRuntime::GetWifiPowerSave() const
{
    return m_wifiPowerSave;
}

void ConfigRuntime::SetCurrentEspnowChannel(uint8_t channel)
{
    m_currentEspnowChannel = channel;
}

uint8_t ConfigRuntime::GetCurrentNodeID()
{
    return m_currentNodeID;
}

void ConfigRuntime::SetCurrentNodeID(uint8_t nodeID)
{
    m_currentNodeID = nodeID;
}

uint16_t ConfigRuntime::GetAnchorPositionX()
{
    return m_anchorPositionX;
}

void ConfigRuntime::SetAnchorPositionX(uint16_t positionX)
{
    m_anchorPositionX = positionX;
}

uint16_t ConfigRuntime::GetAnchorPositionY()
{
    return m_anchorPositionY;
}

void ConfigRuntime::SetAnchorPositionY(uint16_t positionY)
{
    m_anchorPositionY = positionY;
}
