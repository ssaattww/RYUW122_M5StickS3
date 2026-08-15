#include "Ryuw122Initializer.h"

#include "ConfigRuntime.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

namespace
{
    constexpr char Ryuw122NetworkId[] = "UWB00001";
    constexpr uint32_t Ryuw122ModeChangeWaitMs = 2000U;
}

Ryuw122Initializer::Ryuw122Initializer(
    IRyuw122Port& port,
    ConfigRuntime& configRuntime)
    : m_port(port),
      m_configRuntime(configRuntime)
{
}

EnRyuw122InitResult Ryuw122Initializer::Begin()
{
    if (!m_port.Begin())
    {
        return EnRyuw122InitResult::SerialBeginFailed;
    }

    m_port.Recover();
    if (!m_port.Test())
    {
        m_port.Recover();
        if (!m_port.Test())
        {
            return EnRyuw122InitResult::CommunicationFailed;
        }
    }

    EnRyuw122InitResult result = ConfigureMode();
    if (result != EnRyuw122InitResult::Ok)
    {
        return result;
    }

    result = ConfigureNetworkId();
    if (result != EnRyuw122InitResult::Ok)
    {
        return result;
    }

    return ConfigureAddress();
}

const char* Ryuw122Initializer::GetResultName(EnRyuw122InitResult result)
{
    switch (result)
    {
    case EnRyuw122InitResult::Ok:
        return "OK";
    case EnRyuw122InitResult::SerialBeginFailed:
        return "SERIAL";
    case EnRyuw122InitResult::CommunicationFailed:
        return "AT";
    case EnRyuw122InitResult::ModeReadFailed:
        return "MODE_READ";
    case EnRyuw122InitResult::ModeWriteFailed:
        return "MODE_WRITE";
    case EnRyuw122InitResult::NetworkIdReadFailed:
        return "NETWORK_READ";
    case EnRyuw122InitResult::NetworkIdWriteFailed:
        return "NETWORK_WRITE";
    case EnRyuw122InitResult::AddressReadFailed:
        return "ADDRESS_READ";
    case EnRyuw122InitResult::AddressWriteFailed:
        return "ADDRESS_WRITE";
    }

    return "UNKNOWN";
}

void Ryuw122Initializer::BuildAddress(char* address) const
{
    const char rolePrefix =
        m_configRuntime.GetRunMode() == EnRunMode::Tag ? 'T' : 'A';
    snprintf(
        address,
        9,
        "%c%07u",
        rolePrefix,
        static_cast<unsigned int>(m_configRuntime.GetCurrentNodeID()));
}

EnRyuw122InitResult Ryuw122Initializer::ConfigureMode()
{
    const EnRyuw122PortMode desiredMode =
        m_configRuntime.GetRunMode() == EnRunMode::Tag
            ? EnRyuw122PortMode::Tag
            : EnRyuw122PortMode::Anchor;
    const EnRyuw122PortMode currentMode = m_port.GetMode();
    if (currentMode == EnRyuw122PortMode::Unknown)
    {
        return EnRyuw122InitResult::ModeReadFailed;
    }

    if (currentMode != desiredMode)
    {
        if (!m_port.SetMode(desiredMode))
        {
            return EnRyuw122InitResult::ModeWriteFailed;
        }
        delay(Ryuw122ModeChangeWaitMs);
    }

    return EnRyuw122InitResult::Ok;
}

EnRyuw122InitResult Ryuw122Initializer::ConfigureNetworkId()
{
    char currentNetworkId[9] = {};
    if (!m_port.GetNetworkId(currentNetworkId))
    {
        return EnRyuw122InitResult::NetworkIdReadFailed;
    }

    if (strcmp(currentNetworkId, Ryuw122NetworkId) != 0 &&
        !m_port.SetNetworkId(Ryuw122NetworkId))
    {
        return EnRyuw122InitResult::NetworkIdWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}

EnRyuw122InitResult Ryuw122Initializer::ConfigureAddress()
{
    char desiredAddress[9] = {};
    BuildAddress(desiredAddress);

    char currentAddress[9] = {};
    if (!m_port.GetAddress(currentAddress))
    {
        return EnRyuw122InitResult::AddressReadFailed;
    }

    if (strcmp(currentAddress, desiredAddress) != 0 &&
        !m_port.SetAddress(desiredAddress))
    {
        return EnRyuw122InitResult::AddressWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}
