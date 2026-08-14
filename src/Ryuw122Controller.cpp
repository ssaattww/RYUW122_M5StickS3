#include "Ryuw122Controller.h"

#include "ConfigRuntime.h"

#include <Arduino.h>
#include <RYUW122.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    constexpr uint8_t Ryuw122RxPin = 1;
    constexpr uint8_t Ryuw122TxPin = 7;
    constexpr char Ryuw122NetworkId[] = "UWB00001";
    constexpr size_t Ryuw122LineCapacity = 96;
    constexpr size_t Ryuw122ResponseQueueCapacity = 4;

    /**
     * @brief 既定のマイクロ秒時刻を取得します。
     *
     * @return 現在のマイクロ秒時刻
     */
    uint32_t GetDefaultTimeUs()
    {
        return micros();
    }

    /**
     * @brief 32bit時刻の折り返しを考慮して所定時間の経過を確認します。
     *
     * @param nowUs 現在時刻
     * @param startedAtUs 開始時刻
     * @param durationUs 判定する経過時間
     * @return 所定時間が経過した場合はtrue、それ以外はfalse
     */
    bool HasElapsed(
        uint32_t nowUs,
        uint32_t startedAtUs,
        uint32_t durationUs)
    {
        return static_cast<uint32_t>(nowUs - startedAtUs) >= durationUs;
    }

    /**
     * @brief 8文字のRYUW122端末アドレスか確認します。
     *
     * @param address 確認するアドレス
     * @return 8文字のアドレスの場合はtrue、それ以外はfalse
     */
    bool IsValidAddress(const char* address)
    {
        return address != nullptr && strlen(address) == 8U;
    }

    /**
     * @brief 区切り位置までの文字列を固定長領域へコピーします。
     *
     * @param begin コピー開始位置
     * @param end コピー終了位置
     * @param destination コピー先
     * @param destinationSize コピー先のbyte数
     * @return 文字列がコピー先へ収まった場合はtrue、それ以外はfalse
     */
    bool CopyField(
        const char* begin,
        const char* end,
        char* destination,
        size_t destinationSize)
    {
        if (begin == nullptr || end == nullptr || end < begin ||
            destination == nullptr || destinationSize == 0U)
        {
            return false;
        }

        const size_t length = static_cast<size_t>(end - begin);
        if (length >= destinationSize)
        {
            return false;
        }

        memcpy(destination, begin, length);
        destination[length] = '\0';
        return true;
    }

    /**
     * @brief 10進整数fieldを範囲検査付きで解析します。
     *
     * @param begin field開始位置
     * @param end field終了位置
     * @param minimum 許容する最小値
     * @param maximum 許容する最大値
     * @param value 解析結果の格納先
     * @return 範囲内の整数を解析できた場合はtrue、それ以外はfalse
     */
    bool ParseIntegerField(
        const char* begin,
        const char* end,
        long minimum,
        long maximum,
        long& value)
    {
        char field[16] = {};
        if (!CopyField(begin, end, field, sizeof(field)) || field[0] == '\0')
        {
            return false;
        }

        errno = 0;
        char* parsedEnd = nullptr;
        const long parsed = strtol(field, &parsedEnd, 10);
        if (errno == ERANGE || parsedEnd == field || *parsedEnd != '\0' ||
            parsed < minimum || parsed > maximum)
        {
            return false;
        }

        value = parsed;
        return true;
    }

    /**
     * @brief ANCHOR受信行を空payload対応で解析します。
     *
     * @param line 解析する受信行
     * @param response 解析結果の格納先
     * @return 妥当な測距応答を解析できた場合はtrue、それ以外はfalse
     */
    bool ParseAnchorResponse(
        const char* line,
        Ryuw122PortResponse& response)
    {
        constexpr char Prefix[] = "+ANCHOR_RCV=";
        constexpr size_t PrefixLength = sizeof(Prefix) - 1U;
        if (line == nullptr || strncmp(line, Prefix, PrefixLength) != 0)
        {
            return false;
        }

        const char* fields[5] = {line + PrefixLength, nullptr, nullptr, nullptr, nullptr};
        const char* fieldEnds[5] = {};
        const char* cursor = fields[0];
        for (size_t index = 0; index < 4U; ++index)
        {
            const char* comma = strchr(cursor, ',');
            if (comma == nullptr)
            {
                return false;
            }
            fieldEnds[index] = comma;
            fields[index + 1U] = comma + 1;
            cursor = comma + 1;
        }
        fieldEnds[4] = line + strlen(line);

        char tagAddress[9] = {};
        long payloadLength = 0;
        long distanceCm = 0;
        long uwbRssi = 0;
        if (!CopyField(
                fields[0],
                fieldEnds[0],
                tagAddress,
                sizeof(tagAddress)) ||
            !IsValidAddress(tagAddress) ||
            !ParseIntegerField(
                fields[1],
                fieldEnds[1],
                0,
                RYUW122_MAX_PAYLOAD_LENGTH,
                payloadLength) ||
            static_cast<size_t>(fieldEnds[2] - fields[2]) !=
                static_cast<size_t>(payloadLength) ||
            !ParseIntegerField(
                fields[3],
                fieldEnds[3],
                0,
                static_cast<long>(UINT32_MAX / 10U),
                distanceCm) ||
            !ParseIntegerField(
                fields[4],
                fieldEnds[4],
                INT16_MIN,
                INT16_MAX,
                uwbRssi))
        {
            return false;
        }

        memcpy(response.tagAddress, tagAddress, sizeof(response.tagAddress));
        response.isSuccess = true;
        response.distanceCm = static_cast<int32_t>(distanceCm);
        response.uwbRssi = static_cast<int16_t>(uwbRssi);
        return true;
    }

    /**
     * @brief HardwareSerialとRYUW122ライブラリを接続する実機用portです。
     */
    class Ryuw122HardwarePort final : public IRyuw122Port
    {
    public:
        /**
         * @brief G7 TX、G1 RX、115200bpsの実機用portを生成します。
         *
         * @param serial RYUW122との通信に使用するHardwareSerial
         */
        explicit Ryuw122HardwarePort(HardwareSerial& serial)
            : m_serial(serial),
              m_ryuw122(
                  Ryuw122TxPin,
                  Ryuw122RxPin,
                  &serial,
                  RYUW122BaudRate::B_115200)
        {
        }

        /**
         * @brief RYUW122ライブラリでUARTを初期化します。
         *
         * @return 初期化できた場合はtrue、それ以外はfalse
         */
        bool Begin() override
        {
            m_lineLength = 0;
            m_lineOverflow = false;
            m_responseQueueHead = 0;
            m_responseQueueCount = 0;
            return m_ryuw122.begin();
        }

        /**
         * @brief RYUW122ライブラリでAT通信を確認します。
         *
         * @return 応答を確認できた場合はtrue、それ以外はfalse
         */
        bool Test() override
        {
            return m_ryuw122.test();
        }

        /**
         * @brief RYUW122ライブラリから現在の動作モードを取得します。
         *
         * @return 現在の動作モード
         */
        EnRyuw122PortMode GetMode() override
        {
            switch (m_ryuw122.getMode())
            {
            case RYUW122Mode::TAG:
                return EnRyuw122PortMode::Tag;
            case RYUW122Mode::ANCHOR:
                return EnRyuw122PortMode::Anchor;
            default:
                return EnRyuw122PortMode::Unknown;
            }
        }

        /**
         * @brief RYUW122ライブラリで動作モードを設定します。
         *
         * @param mode 設定する動作モード
         * @return 設定できた場合はtrue、それ以外はfalse
         */
        bool SetMode(EnRyuw122PortMode mode) override
        {
            if (mode == EnRyuw122PortMode::Unknown)
            {
                return false;
            }
            return m_ryuw122.setMode(
                mode == EnRyuw122PortMode::Tag
                    ? RYUW122Mode::TAG
                    : RYUW122Mode::ANCHOR);
        }

        /**
         * @brief RYUW122ライブラリからネットワークIDを取得します。
         *
         * @param networkId 取得したIDの格納先
         * @return 取得できた場合はtrue、それ以外はfalse
         */
        bool GetNetworkId(char* networkId) override
        {
            return m_ryuw122.getNetworkId(networkId);
        }

        /**
         * @brief RYUW122ライブラリでネットワークIDを設定します。
         *
         * @param networkId 設定するID
         * @return 設定できた場合はtrue、それ以外はfalse
         */
        bool SetNetworkId(const char* networkId) override
        {
            return m_ryuw122.setNetworkId(networkId);
        }

        /**
         * @brief RYUW122ライブラリから端末アドレスを取得します。
         *
         * @param address 取得したアドレスの格納先
         * @return 取得できた場合はtrue、それ以外はfalse
         */
        bool GetAddress(char* address) override
        {
            return m_ryuw122.getAddress(address);
        }

        /**
         * @brief RYUW122ライブラリで端末アドレスを設定します。
         *
         * @param address 設定するアドレス
         * @return 設定できた場合はtrue、それ以外はfalse
         */
        bool SetAddress(const char* address) override
        {
            return m_ryuw122.setAddress(address);
        }

        /**
         * @brief 待機を伴うライブラリ送信を使わず測距コマンドをUARTへ投入します。
         *
         * @param tagAddress 測距対象の8文字TAGアドレス
         * @return コマンド全体を投入できた場合はtrue、それ以外はfalse
         */
        bool StartRanging(const char* tagAddress) override
        {
            if (!IsValidAddress(tagAddress))
            {
                return false;
            }

            char command[40] = {};
            const int commandLength = snprintf(
                command,
                sizeof(command),
                "AT+ANCHOR_SEND=%s,0,\r\n",
                tagAddress);
            if (commandLength <= 0 ||
                commandLength >= static_cast<int>(sizeof(command)) ||
                m_serial.availableForWrite() < commandLength)
            {
                return false;
            }

            return m_serial.write(
                       reinterpret_cast<const uint8_t*>(command),
                       static_cast<size_t>(commandLength)) ==
                static_cast<size_t>(commandLength);
        }

        /**
         * @brief UARTの到着済みbyteだけを読み、完全な行を逐次処理します。
         */
        void Update() override
        {
            while (m_serial.available() > 0)
            {
                const int value = m_serial.read();
                if (value < 0)
                {
                    break;
                }

                const char character = static_cast<char>(value);
                if (character == '\r')
                {
                    continue;
                }
                if (character == '\n')
                {
                    if (!m_lineOverflow && m_lineLength > 0U)
                    {
                        m_line[m_lineLength] = '\0';
                        ProcessLine(m_line);
                    }
                    m_lineLength = 0;
                    m_lineOverflow = false;
                    continue;
                }

                if (!m_lineOverflow)
                {
                    if (m_lineLength + 1U < sizeof(m_line))
                    {
                        m_line[m_lineLength++] = character;
                    }
                    else
                    {
                        m_lineOverflow = true;
                    }
                }
            }
        }

        /**
         * @brief 解析済み測距応答を1件取得します。
         *
         * @param response 取得した応答の格納先
         * @return 応答を取得した場合はtrue、それ以外はfalse
         */
        bool TryTakeResponse(Ryuw122PortResponse& response) override
        {
            if (m_responseQueueCount == 0U)
            {
                return false;
            }
            response = m_responseQueue[m_responseQueueHead];
            m_responseQueueHead =
                (m_responseQueueHead + 1U) % Ryuw122ResponseQueueCapacity;
            --m_responseQueueCount;
            return true;
        }

    private:
        /**
         * @brief 解析済み応答を固定長FIFOへ到着順で追加します。
         * FIFO満杯時は保持済み応答の順序を維持し、新着応答を破棄します。
         *
         * @param response 追加する測距応答
         * @return FIFOへ追加した場合はtrue、満杯で破棄した場合はfalse
         */
        bool EnqueueResponse(const Ryuw122PortResponse& response)
        {
            if (m_responseQueueCount >= Ryuw122ResponseQueueCapacity)
            {
                return false;
            }

            const size_t tail =
                (m_responseQueueHead + m_responseQueueCount) %
                Ryuw122ResponseQueueCapacity;
            m_responseQueue[tail] = response;
            ++m_responseQueueCount;
            return true;
        }

        /**
         * @brief RYUW122の受信行から測距応答または失敗応答を保存します。
         *
         * @param line 処理する受信行
         */
        void ProcessLine(const char* line)
        {
            Ryuw122PortResponse response{};
            if (ParseAnchorResponse(line, response))
            {
                EnqueueResponse(response);
                return;
            }

            if (strncmp(line, "+ANCHOR_RCV=", 12U) == 0 ||
                strncmp(line, "+ERR", 4U) == 0 ||
                strncmp(line, "+ERROR", 6U) == 0)
            {
                EnqueueResponse(response);
            }
        }

        HardwareSerial& m_serial;
        RYUW122 m_ryuw122;
        char m_line[Ryuw122LineCapacity] = {};
        size_t m_lineLength = 0;
        bool m_lineOverflow = false;
        Ryuw122PortResponse
            m_responseQueue[Ryuw122ResponseQueueCapacity] = {};
        size_t m_responseQueueHead = 0;
        size_t m_responseQueueCount = 0;
    };
}

Ryuw122Controller::Ryuw122Controller(
    HardwareSerial& serial,
    ConfigRuntime& configRuntime,
    Ryuw122TimeProvider timeProvider)
    : m_configRuntime(configRuntime),
      m_port(new Ryuw122HardwarePort(serial)),
      m_timeProvider(timeProvider != nullptr ? timeProvider : GetDefaultTimeUs),
      m_ownsPort(true)
{
}

Ryuw122Controller::Ryuw122Controller(
    IRyuw122Port& port,
    ConfigRuntime& configRuntime,
    Ryuw122TimeProvider timeProvider)
    : m_configRuntime(configRuntime),
      m_port(&port),
      m_timeProvider(timeProvider != nullptr ? timeProvider : GetDefaultTimeUs),
      m_ownsPort(false)
{
}

Ryuw122Controller::~Ryuw122Controller()
{
    if (m_ownsPort)
    {
        delete m_port;
    }
}

EnRyuw122InitResult Ryuw122Controller::Begin()
{
    m_isReady = false;
    m_hasResult = false;
    m_rangingState = EnRangingState::Idle;
    memset(m_activeTagAddress, 0, sizeof(m_activeTagAddress));
    memset(m_drainTagAddress, 0, sizeof(m_drainTagAddress));

    if (!m_port->Begin())
    {
        m_lastResult = EnRyuw122InitResult::SerialBeginFailed;
        return m_lastResult;
    }

    if (!m_port->Test())
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
    if (!m_isReady)
    {
        return;
    }

    const uint32_t nowUs = m_timeProvider();
    if (m_rangingState == EnRangingState::WaitingForResponse &&
        HasElapsed(nowUs, m_rangingStartedAtUs, m_rangingTimeoutUs))
    {
        CompleteRanging(
            EnRyuw122RangingStatus::TimedOut,
            0,
            0,
            nowUs);
        BeginLateResponseDrain(nowUs);
    }
    else if (m_rangingState == EnRangingState::DrainingLateResponse &&
             HasElapsed(
                 nowUs,
                 m_drainStartedAtUs,
                 m_lateResponseDrainTimeoutUs))
    {
        m_rangingState = EnRangingState::Idle;
        memset(m_drainTagAddress, 0, sizeof(m_drainTagAddress));
    }

    m_port->Update();
    Ryuw122PortResponse response{};
    while (m_port->TryTakeResponse(response))
    {
        const bool hasAddress = response.tagAddress[0] != '\0';
        if (m_rangingState == EnRangingState::DrainingLateResponse)
        {
            if (!hasAddress ||
                strcmp(response.tagAddress, m_drainTagAddress) == 0)
            {
                m_rangingState = EnRangingState::Idle;
                memset(m_drainTagAddress, 0, sizeof(m_drainTagAddress));
            }
            continue;
        }

        if (m_rangingState != EnRangingState::WaitingForResponse ||
            (hasAddress &&
             strcmp(response.tagAddress, m_activeTagAddress) != 0))
        {
            continue;
        }

        const uint32_t completedAtUs = m_timeProvider();
        if (!response.isSuccess || response.distanceCm < 0 || !hasAddress)
        {
            CompleteRanging(
                EnRyuw122RangingStatus::Failed,
                0,
                0,
                completedAtUs);
        }
        else
        {
            CompleteRanging(
                EnRyuw122RangingStatus::Success,
                static_cast<uint32_t>(response.distanceCm) * 10U,
                response.uwbRssi,
                completedAtUs);
        }
    }
}

bool Ryuw122Controller::StartRanging(const char* tagAddress)
{
    if (!m_isReady || m_configRuntime.GetRunMode() != EnRunMode::Anchor ||
        !IsValidAddress(tagAddress) || IsBusy())
    {
        return false;
    }

    memcpy(m_activeTagAddress, tagAddress, sizeof(m_activeTagAddress));
    m_rangingStartedAtUs = m_timeProvider();
    m_rangingState = EnRangingState::WaitingForResponse;
    if (!m_port->StartRanging(tagAddress))
    {
        CompleteRanging(
            EnRyuw122RangingStatus::Failed,
            0,
            0,
            m_timeProvider());
    }
    return true;
}

bool Ryuw122Controller::TryTakeResult(Ryuw122RangingResult& result)
{
    if (!m_hasResult)
    {
        return false;
    }

    result = m_result;
    m_hasResult = false;
    return true;
}

bool Ryuw122Controller::IsBusy() const
{
    return m_hasResult || m_rangingState != EnRangingState::Idle;
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
    const EnRyuw122PortMode desiredMode =
        m_configRuntime.GetRunMode() == EnRunMode::Tag
            ? EnRyuw122PortMode::Tag
            : EnRyuw122PortMode::Anchor;
    const EnRyuw122PortMode currentMode = m_port->GetMode();
    if (currentMode == EnRyuw122PortMode::Unknown)
    {
        return EnRyuw122InitResult::ModeReadFailed;
    }

    if (currentMode != desiredMode && !m_port->SetMode(desiredMode))
    {
        return EnRyuw122InitResult::ModeWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}

EnRyuw122InitResult Ryuw122Controller::ConfigureNetworkId()
{
    char currentNetworkId[9] = {};
    if (!m_port->GetNetworkId(currentNetworkId))
    {
        return EnRyuw122InitResult::NetworkIdReadFailed;
    }

    if (strcmp(currentNetworkId, Ryuw122NetworkId) != 0 &&
        !m_port->SetNetworkId(Ryuw122NetworkId))
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
    if (!m_port->GetAddress(currentAddress))
    {
        return EnRyuw122InitResult::AddressReadFailed;
    }

    if (strcmp(currentAddress, desiredAddress) != 0 &&
        !m_port->SetAddress(desiredAddress))
    {
        return EnRyuw122InitResult::AddressWriteFailed;
    }

    return EnRyuw122InitResult::Ok;
}

void Ryuw122Controller::CompleteRanging(
    EnRyuw122RangingStatus status,
    uint32_t distanceMm,
    int16_t uwbRssi,
    uint32_t completedAtUs)
{
    memset(&m_result, 0, sizeof(m_result));
    memcpy(
        m_result.tagAddress,
        m_activeTagAddress,
        sizeof(m_result.tagAddress));
    m_result.status = status;
    m_result.distanceMm = distanceMm;
    m_result.uwbRssi = uwbRssi;
    m_result.startedAtUs = m_rangingStartedAtUs;
    m_result.completedAtUs = completedAtUs;
    m_hasResult = true;
    m_rangingState = EnRangingState::Idle;
    memset(m_activeTagAddress, 0, sizeof(m_activeTagAddress));
}

void Ryuw122Controller::BeginLateResponseDrain(uint32_t nowUs)
{
    memcpy(
        m_drainTagAddress,
        m_result.tagAddress,
        sizeof(m_drainTagAddress));
    m_drainStartedAtUs = nowUs;
    m_rangingState = EnRangingState::DrainingLateResponse;
}
