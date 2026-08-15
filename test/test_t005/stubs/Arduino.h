#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

using byte = uint8_t;

constexpr uint8_t INPUT = 0;
constexpr uint8_t OUTPUT = 1;
constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;

/**
 * @brief native testで記録する実機adapter event種別を表します。
 */
enum class EnTestHardwareEvent : uint8_t
{
    UartBegin,
    DigitalWrite,
    PinMode,
    Delay,
    Test,
    SetMode,
    GetNetworkId,
};

/**
 * @brief native testで記録した実機adapter eventを保持します。
 */
struct TestHardwareEvent
{
    EnTestHardwareEvent type = EnTestHardwareEvent::UartBegin;
    uint32_t firstValue = 0;
    uint32_t secondValue = 0;
};

extern std::vector<TestHardwareEvent> hardwareEvents;

/**
 * @brief test用のマイクロ秒時刻を返します。
 *
 * @return testで設定したマイクロ秒時刻
 */
uint32_t micros();

/**
 * @brief native testでGPIOの出力値を記録します。
 *
 * @param pin 対象GPIO
 * @param value 設定する出力値
 */
void digitalWrite(uint8_t pin, uint8_t value);

/**
 * @brief native testでGPIO modeを記録します。
 *
 * @param pin 対象GPIO
 * @param mode 設定するGPIO mode
 */
void pinMode(uint8_t pin, uint8_t mode);

/**
 * @brief native testでblocking待機時間を記録します。
 *
 * @param milliseconds 待機時間ms
 */
void delay(uint32_t milliseconds);

/**
 * @brief native testでUART入出力を再現します。
 */
class HardwareSerial
{
public:
    /**
     * @brief UART送信可能byte数を取得します。
     *
     * @return 送信可能byte数
     */
    int availableForWrite() const
    {
        return m_availableForWrite;
    }

    /**
     * @brief UART受信可能byte数を取得します。
     *
     * @return 受信可能byte数
     */
    int available() const
    {
        return static_cast<int>(m_received.size());
    }

    /**
     * @brief UART受信byteを1件取得します。
     *
     * @return 受信byte。空の場合は-1
     */
    int read()
    {
        if (m_received.empty())
        {
            return -1;
        }
        const uint8_t value = m_received.front();
        m_received.pop_front();
        return value;
    }

    /**
     * @brief UART送信内容をtest用bufferへ保存します。
     *
     * @param data 送信するbyte列
     * @param length 送信byte数
     * @return 保存したbyte数
     */
    size_t write(const uint8_t* data, size_t length)
    {
        if (data == nullptr ||
            length > static_cast<size_t>(m_availableForWrite))
        {
            return 0;
        }
        m_written.append(
            reinterpret_cast<const char*>(data),
            length);
        return length;
    }

    /**
     * @brief testでUART受信させる文字列を追加します。
     *
     * @param text 追加する受信文字列
     */
    void InjectReceive(const char* text)
    {
        if (text == nullptr)
        {
            return;
        }
        while (*text != '\0')
        {
            m_received.push_back(static_cast<uint8_t>(*text++));
        }
    }

    /**
     * @brief 保存したUART送信内容を取得します。
     *
     * @return UART送信内容
     */
    const std::string& GetWritten() const
    {
        return m_written;
    }

    /**
     * @brief UART送信可能byte数を設定します。
     *
     * @param availableForWrite 送信可能byte数
     */
    void SetAvailableForWrite(int availableForWrite)
    {
        m_availableForWrite = availableForWrite;
    }

private:
    int m_availableForWrite = 256;
    std::deque<uint8_t> m_received;
    std::string m_written;
};
