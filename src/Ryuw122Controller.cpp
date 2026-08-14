#include "Ryuw122Controller.h"

#include <cstdio>
#include <cstring>

namespace
{
    constexpr uint8_t Ryuw122RxPin = 1;
    constexpr uint8_t Ryuw122TxPin = 7;
    constexpr char Ryuw122NetworkId[] = "UWB00001";
}

Ryuw122Controller::Ryuw122Controller(
    HardwareSerial& serial,
    ConfigRuntime& configRuntime)
    : m_configRuntime(configRuntime),
      m_ryuw122(
          Ryuw122TxPin,
          Ryuw122RxPin,
          &serial,
          RYUW122BaudRate::B_115200)
{
}

EnRyuw122InitResult Ryuw122Controller::Begin()
{
    m_isReady = false;

    if (!m_ryuw122.begin())
    {
        m_lastResult = EnRyuw122InitResult::SerialBeginFailed;
        return m_lastResult;
    }

    if (!m_ryuw122.test())
    {
        m_lastResult = EnRyuw122InitResult::CommunicationFailed;
        return m_lastResult;
    }

    m_lastResult = ConfigureMode();
    if (m_lastResult != EnRyuw122InitResult::Ok)
    {
        return m_lastResult;
    }

    m_lastResult = ConfigureNetworkId();
    if (m_lastResult != EnRyuw122InitResult::Ok)
    {
        return m_lastResult;
    }

    m_lastResult = ConfigureAddress();
    if (m_lastResult != EnRyuw122InitResult::Ok)
    {
        return m_lastResult;
    }

    m_isReady = true;
    return m_lastResult;
}

void Ryuw122Controller::Update()
{
    if (m_isReady)
    {
        m_ryuw122.loop();
    }
}

bool Ryuw122Controller::IsReady() const
{
    return m_isReady;
}

EnRyuw122InitResult Ryuw122Controller::GetLastResult() const
{
    return m_lastResult;
}

const char* Ryuw122Controller::GetResultName(EnRyuw122InitResult result)
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

void Ryuw122Controller::BuildAddress(char* address) const
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

EnRyuw122InitResult Ryuw122Controller::ConfigureMode()
{
    const RYUW122Mode desiredMode =
        m_configRuntime.GetRunMode() == EnRunMode::Tag
            ? RYUW122Mode::TAG
            : RYUW122Mode::ANCHOR;
    const RYUW122Mode currentMode = m_ryuw122.getMode();
    if (currentMode == RYUW122Mode::UNKNOWN)
    {
        return EnRyuw122InitResult::ModeReadFailed;
    }

    if (currentMode != desiredMode && !m_ryuw122.setMode(desiredMode))
    {
        return EnRyuw122InitResult::ModeWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}

EnRyuw122InitResult Ryuw122Controller::ConfigureNetworkId()
{
    char currentNetworkId[9] = {};
    if (!m_ryuw122.getNetworkId(currentNetworkId))
    {
        return EnRyuw122InitResult::NetworkIdReadFailed;
    }

    if (strcmp(currentNetworkId, Ryuw122NetworkId) != 0 &&
        !m_ryuw122.setNetworkId(Ryuw122NetworkId))
    {
        return EnRyuw122InitResult::NetworkIdWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}

EnRyuw122InitResult Ryuw122Controller::ConfigureAddress()
{
    char desiredAddress[9] = {};
    BuildAddress(desiredAddress);

    char currentAddress[9] = {};
    if (!m_ryuw122.getAddress(currentAddress))
    {
        return EnRyuw122InitResult::AddressReadFailed;
    }

    if (strcmp(currentAddress, desiredAddress) != 0 &&
        !m_ryuw122.setAddress(desiredAddress))
    {
        return EnRyuw122InitResult::AddressWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}
