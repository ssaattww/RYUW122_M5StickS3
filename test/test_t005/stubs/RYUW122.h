#pragma once

#include "Arduino.h"

#include <cstring>

#define RYUW122_MAX_PAYLOAD_LENGTH 12

/**
 * @brief test用RYUW122 baud rateを表します。
 */
enum class RYUW122BaudRate : uint32_t
{
    B_115200 = 115200,
};

/**
 * @brief test用RYUW122動作モードを表します。
 */
enum class RYUW122Mode : uint8_t
{
    TAG,
    ANCHOR,
    UNKNOWN,
};

/**
 * @brief native test用RYUW122ライブラリ差し替えです。
 */
class RYUW122
{
public:
    static uint8_t m_lastTxPin;
    static uint8_t m_lastRxPin;
    static RYUW122BaudRate m_lastBaudRate;

    /**
     * @brief pinとbaud rateの構築契約を記録します。
     *
     * @param txPin MCU側TX pin
     * @param rxPin MCU側RX pin
     * @param serial 使用するHardwareSerial
     * @param baudRate 使用するbaud rate
     */
    RYUW122(
        uint8_t txPin,
        uint8_t rxPin,
        HardwareSerial* serial,
        RYUW122BaudRate baudRate)
    {
        (void)serial;
        m_lastTxPin = txPin;
        m_lastRxPin = rxPin;
        m_lastBaudRate = baudRate;
    }

    /**
     * @brief test用初期化を成功させます。
     *
     * @return 常にtrue
     */
    bool begin()
    {
        return true;
    }

    /**
     * @brief test用AT疎通を成功させます。
     *
     * @return 常にtrue
     */
    bool test()
    {
        return true;
    }

    /**
     * @brief test用動作モードを取得します。
     *
     * @return 現在の動作モード
     */
    RYUW122Mode getMode()
    {
        return m_mode;
    }

    /**
     * @brief test用動作モードを設定します。
     *
     * @param mode 設定する動作モード
     * @return 常にtrue
     */
    bool setMode(RYUW122Mode mode)
    {
        m_mode = mode;
        return true;
    }

    /**
     * @brief test用ネットワークIDを取得します。
     *
     * @param networkId 取得先
     * @return 常にtrue
     */
    bool getNetworkId(char* networkId)
    {
        memcpy(networkId, m_networkId, sizeof(m_networkId));
        return true;
    }

    /**
     * @brief test用ネットワークIDを設定します。
     *
     * @param networkId 設定するID
     * @return 常にtrue
     */
    bool setNetworkId(const char* networkId)
    {
        memcpy(m_networkId, networkId, sizeof(m_networkId));
        return true;
    }

    /**
     * @brief test用端末アドレスを取得します。
     *
     * @param address 取得先
     * @return 常にtrue
     */
    bool getAddress(char* address)
    {
        memcpy(address, m_address, sizeof(m_address));
        return true;
    }

    /**
     * @brief test用端末アドレスを設定します。
     *
     * @param address 設定するアドレス
     * @return 常にtrue
     */
    bool setAddress(const char* address)
    {
        memcpy(m_address, address, sizeof(m_address));
        return true;
    }

private:
    RYUW122Mode m_mode = RYUW122Mode::ANCHOR;
    char m_networkId[9] = "UWB00001";
    char m_address[9] = "A0000001";
};
