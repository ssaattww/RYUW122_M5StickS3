#include <unity.h>

#include "Arduino.h"
#include "ConfigRuntime.h"
#include "RYUW122.h"
#include "Ryuw122Controller.h"

#include <cstring>
#include <deque>

uint8_t RYUW122::m_lastTxPin = 0;
uint8_t RYUW122::m_lastRxPin = 0;
RYUW122BaudRate RYUW122::m_lastBaudRate =
    RYUW122BaudRate::B_115200;

namespace
{
    uint32_t fakeTimeUs = 0;

    /**
     * @brief test用の単調増加マイクロ秒時刻を返します。
     *
     * @return 設定済みtest時刻
     */
    uint32_t GetFakeTimeUs()
    {
        return fakeTimeUs;
    }

    /**
     * @brief controller差し替え用のRYUW122 portです。
     */
    class FakeRyuw122Port final : public IRyuw122Port
    {
    public:
        bool beginResult = true;
        bool testResult = true;
        bool startResult = true;
        EnRyuw122PortMode mode = EnRyuw122PortMode::Anchor;
        char networkId[9] = "UWB00001";
        char address[9] = "A0000001";
        char startedTagAddress[9] = {};
        uint32_t startCount = 0;

        /**
         * @brief test用port初期化結果を返します。
         *
         * @return 設定済み初期化結果
         */
        bool Begin() override
        {
            return beginResult;
        }

        /**
         * @brief test用AT疎通結果を返します。
         *
         * @return 設定済み疎通結果
         */
        bool Test() override
        {
            return testResult;
        }

        /**
         * @brief test用動作モードを取得します。
         *
         * @return 現在の動作モード
         */
        EnRyuw122PortMode GetMode() override
        {
            return mode;
        }

        /**
         * @brief test用動作モードを設定します。
         *
         * @param nextMode 設定する動作モード
         * @return 常にtrue
         */
        bool SetMode(EnRyuw122PortMode nextMode) override
        {
            mode = nextMode;
            return true;
        }

        /**
         * @brief test用ネットワークIDを取得します。
         *
         * @param value 取得先
         * @return 常にtrue
         */
        bool GetNetworkId(char* value) override
        {
            memcpy(value, networkId, sizeof(networkId));
            return true;
        }

        /**
         * @brief test用ネットワークIDを設定します。
         *
         * @param value 設定するID
         * @return 常にtrue
         */
        bool SetNetworkId(const char* value) override
        {
            memcpy(networkId, value, sizeof(networkId));
            return true;
        }

        /**
         * @brief test用端末アドレスを取得します。
         *
         * @param value 取得先
         * @return 常にtrue
         */
        bool GetAddress(char* value) override
        {
            memcpy(value, address, sizeof(address));
            return true;
        }

        /**
         * @brief test用端末アドレスを設定します。
         *
         * @param value 設定するアドレス
         * @return 常にtrue
         */
        bool SetAddress(const char* value) override
        {
            memcpy(address, value, sizeof(address));
            return true;
        }

        /**
         * @brief test用測距開始結果と対象TAGを記録します。
         *
         * @param tagAddress 測距対象TAG
         * @return 設定済み開始結果
         */
        bool StartRanging(const char* tagAddress) override
        {
            ++startCount;
            memcpy(
                startedTagAddress,
                tagAddress,
                sizeof(startedTagAddress));
            return startResult;
        }

        /**
         * @brief test用port更新を行います。
         */
        void Update() override
        {
        }

        /**
         * @brief queueからtest用応答を1件取得します。
         *
         * @param response 取得先
         * @return 応答を取得した場合はtrue、それ以外はfalse
         */
        bool TryTakeResponse(Ryuw122PortResponse& response) override
        {
            if (m_responses.empty())
            {
                return false;
            }
            response = m_responses.front();
            m_responses.pop_front();
            return true;
        }

        /**
         * @brief test用測距応答をqueueへ追加します。
         *
         * @param tagAddress 応答元TAG
         * @param isSuccess 成否
         * @param distanceCm 距離cm
         * @param uwbRssi UWB RSSI
         */
        void QueueResponse(
            const char* tagAddress,
            bool isSuccess,
            int32_t distanceCm,
            int16_t uwbRssi)
        {
            Ryuw122PortResponse response{};
            if (tagAddress != nullptr)
            {
                memcpy(
                    response.tagAddress,
                    tagAddress,
                    sizeof(response.tagAddress));
            }
            response.isSuccess = isSuccess;
            response.distanceCm = distanceCm;
            response.uwbRssi = uwbRssi;
            m_responses.push_back(response);
        }

    private:
        std::deque<Ryuw122PortResponse> m_responses;
    };

    /**
     * @brief test対象controllerを初期化します。
     *
     * @param controller 初期化するcontroller
     */
    void BeginController(Ryuw122Controller& controller)
    {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122InitResult::Ok),
            static_cast<uint8_t>(controller.Begin()));
        TEST_ASSERT_TRUE(controller.IsReady());
    }

    /**
     * @brief 非同期測距の成功結果と時刻を検証します。
     */
    void TestSuccessfulRanging()
    {
        ConfigRuntime config;
        FakeRyuw122Port port;
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 1000;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        TEST_ASSERT_TRUE(controller.IsBusy());
        TEST_ASSERT_EQUAL_STRING("T0000002", port.startedTagAddress);

        fakeTimeUs = 4200;
        port.QueueResponse("T0000002", true, 123, -74);
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_STRING("T0000002", result.tagAddress);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Success),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_UINT32(1230, result.distanceMm);
        TEST_ASSERT_EQUAL_INT16(-74, result.uwbRssi);
        TEST_ASSERT_EQUAL_UINT32(1000, result.startedAtUs);
        TEST_ASSERT_EQUAL_UINT32(4200, result.completedAtUs);
        TEST_ASSERT_FALSE(controller.IsBusy());
    }

    /**
     * @brief UART投入失敗を測距失敗結果として取得できることを検証します。
     */
    void TestStartFailureResult()
    {
        ConfigRuntime config;
        FakeRyuw122Port port;
        port.startResult = false;
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 10;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Failed),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_UINT32(10, result.startedAtUs);
        TEST_ASSERT_EQUAL_UINT32(10, result.completedAtUs);
    }

    /**
     * @brief RYUW122失敗応答を測距失敗結果として取得できることを検証します。
     */
    void TestResponseFailureResult()
    {
        ConfigRuntime config;
        FakeRyuw122Port port;
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 20;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        fakeTimeUs = 30;
        port.QueueResponse(nullptr, false, 0, 0);
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Failed),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_STRING("T0000002", result.tagAddress);
        TEST_ASSERT_EQUAL_UINT32(20, result.startedAtUs);
        TEST_ASSERT_EQUAL_UINT32(30, result.completedAtUs);
    }

    /**
     * @brief 300ms timeoutと排出期限を検証します。
     */
    void TestTimeoutAndDrainDeadline()
    {
        ConfigRuntime config;
        FakeRyuw122Port port;
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 100;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        fakeTimeUs = 300099;
        controller.Update();
        Ryuw122RangingResult result{};
        TEST_ASSERT_FALSE(controller.TryTakeResult(result));

        fakeTimeUs = 300100;
        controller.Update();
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::TimedOut),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_UINT32(100, result.startedAtUs);
        TEST_ASSERT_EQUAL_UINT32(300100, result.completedAtUs);
        TEST_ASSERT_TRUE(controller.IsBusy());
        TEST_ASSERT_FALSE(controller.StartRanging("T0000003"));

        fakeTimeUs = 600100;
        controller.Update();
        TEST_ASSERT_FALSE(controller.IsBusy());
        TEST_ASSERT_TRUE(controller.StartRanging("T0000003"));
    }

    /**
     * @brief timeout後の遅延応答を次の測距へ誤帰属させないことを検証します。
     */
    void TestLateResponseDrain()
    {
        ConfigRuntime config;
        FakeRyuw122Port port;
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 0;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        fakeTimeUs = 300000;
        controller.Update();
        Ryuw122RangingResult timeoutResult{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(timeoutResult));

        fakeTimeUs = 310000;
        port.QueueResponse("T0000002", true, 999, -1);
        controller.Update();
        TEST_ASSERT_FALSE(controller.IsBusy());
        TEST_ASSERT_FALSE(controller.TryTakeResult(timeoutResult));

        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        fakeTimeUs = 311000;
        port.QueueResponse("T0000002", true, 45, -70);
        controller.Update();
        Ryuw122RangingResult nextResult{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(nextResult));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Success),
            static_cast<uint8_t>(nextResult.status));
        TEST_ASSERT_EQUAL_UINT32(450, nextResult.distanceMm);
    }

    /**
     * @brief 測距中と未取得結果がbusyとして次要求を拒否することを検証します。
     */
    void TestBusyBoundary()
    {
        ConfigRuntime config;
        FakeRyuw122Port port;
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 10;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        TEST_ASSERT_FALSE(controller.StartRanging("T0000003"));
        TEST_ASSERT_EQUAL_UINT32(1, port.startCount);

        port.QueueResponse("T0000002", true, 10, -80);
        controller.Update();
        TEST_ASSERT_FALSE(controller.StartRanging("T0000003"));
        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_TRUE(controller.StartRanging("T0000003"));
        TEST_ASSERT_EQUAL_UINT32(2, port.startCount);
    }

    /**
     * @brief TAG動作モードではANCHOR測距を開始しないことを検証します。
     */
    void TestAnchorModeBoundary()
    {
        ConfigRuntime config;
        config.SetRunMode(EnRunMode::Tag);
        FakeRyuw122Port port;
        port.mode = EnRyuw122PortMode::Tag;
        memcpy(port.address, "T0000001", sizeof(port.address));
        Ryuw122Controller controller(port, config, GetFakeTimeUs);
        BeginController(controller);

        TEST_ASSERT_FALSE(controller.StartRanging("T0000002"));
        TEST_ASSERT_EQUAL_UINT32(0, port.startCount);
    }

    /**
     * @brief 実機adapterのG7 TX、G1 RX、115200bpsと空payload解析を検証します。
     */
    void TestHardwarePortContract()
    {
        ConfigRuntime config;
        HardwareSerial serial;
        Ryuw122Controller controller(serial, config, GetFakeTimeUs);
        BeginController(controller);

        TEST_ASSERT_EQUAL_UINT8(7, RYUW122::m_lastTxPin);
        TEST_ASSERT_EQUAL_UINT8(1, RYUW122::m_lastRxPin);
        TEST_ASSERT_EQUAL_UINT32(
            115200,
            static_cast<uint32_t>(RYUW122::m_lastBaudRate));

        fakeTimeUs = 100;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        TEST_ASSERT_EQUAL_STRING(
            "AT+ANCHOR_SEND=T0000002,0,\r\n",
            serial.GetWritten().c_str());
        serial.InjectReceive(
            "+OK\r\n+ANCHOR_RCV=T0000002,0,,42,-77\r\n");
        fakeTimeUs = 200;
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Success),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_UINT32(420, result.distanceMm);
        TEST_ASSERT_EQUAL_INT16(-77, result.uwbRssi);
    }

    /**
     * @brief 異なるTAG応答の直後に到着したactive TAG応答を保持することを検証します。
     */
    void TestHardwarePortForeignThenActiveResponse()
    {
        ConfigRuntime config;
        HardwareSerial serial;
        Ryuw122Controller controller(serial, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 100;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        serial.InjectReceive(
            "+ANCHOR_RCV=T0000003,0,,10,-80\r\n"
            "+ANCHOR_RCV=T0000002,0,,42,-77\r\n");
        fakeTimeUs = 200;
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Success),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_STRING("T0000002", result.tagAddress);
        TEST_ASSERT_EQUAL_UINT32(420, result.distanceMm);
        TEST_ASSERT_FALSE(controller.TryTakeResult(result));
    }

    /**
     * @brief active TAG後の異なるTAG応答を追加結果へ誤帰属しないことを検証します。
     */
    void TestHardwarePortActiveThenForeignResponse()
    {
        ConfigRuntime config;
        HardwareSerial serial;
        Ryuw122Controller controller(serial, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 100;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        serial.InjectReceive(
            "+ANCHOR_RCV=T0000002,0,,42,-77\r\n"
            "+ANCHOR_RCV=T0000003,0,,10,-80\r\n");
        fakeTimeUs = 200;
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Success),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_UINT32(420, result.distanceMm);
        TEST_ASSERT_FALSE(controller.TryTakeResult(result));

        fakeTimeUs = 400000;
        controller.Update();
        TEST_ASSERT_FALSE(controller.TryTakeResult(result));
    }

    /**
     * @brief 実機portの失敗応答を測距失敗として公開することを検証します。
     */
    void TestHardwarePortFailureResponse()
    {
        ConfigRuntime config;
        HardwareSerial serial;
        Ryuw122Controller controller(serial, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 100;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        serial.InjectReceive("+ERR=4\r\n");
        fakeTimeUs = 200;
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::Failed),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_STRING("T0000002", result.tagAddress);
        TEST_ASSERT_FALSE(controller.TryTakeResult(result));
    }

    /**
     * @brief FIFO満杯時に保持済み4応答を維持して新着を破棄することを検証します。
     */
    void TestHardwarePortResponseOverflowPolicy()
    {
        ConfigRuntime config;
        HardwareSerial serial;
        Ryuw122Controller controller(serial, config, GetFakeTimeUs);
        BeginController(controller);

        fakeTimeUs = 0;
        TEST_ASSERT_TRUE(controller.StartRanging("T0000002"));
        serial.InjectReceive(
            "+ANCHOR_RCV=T0000003,0,,10,-80\r\n"
            "+ANCHOR_RCV=T0000004,0,,11,-81\r\n"
            "+ANCHOR_RCV=T0000005,0,,12,-82\r\n"
            "+ANCHOR_RCV=T0000006,0,,13,-83\r\n"
            "+ANCHOR_RCV=T0000002,0,,42,-77\r\n");
        fakeTimeUs = 100;
        controller.Update();

        Ryuw122RangingResult result{};
        TEST_ASSERT_FALSE(controller.TryTakeResult(result));
        fakeTimeUs = 300000;
        controller.Update();
        TEST_ASSERT_TRUE(controller.TryTakeResult(result));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRyuw122RangingStatus::TimedOut),
            static_cast<uint8_t>(result.status));
        TEST_ASSERT_EQUAL_STRING("T0000002", result.tagAddress);
    }
}

/**
 * @brief Unity test前の初期化を行います。
 */
void setUp()
{
    fakeTimeUs = 0;
}

/**
 * @brief Unity test後の後処理を行います。
 */
void tearDown()
{
}

/**
 * @brief Arduino互換のtest用マイクロ秒時刻を返します。
 *
 * @return 設定済みtest時刻
 */
uint32_t micros()
{
    return fakeTimeUs;
}

/**
 * @brief T-005のPlatformIO native test suiteを実行します。
 *
 * @return Unity test結果
 */
int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestSuccessfulRanging);
    RUN_TEST(TestStartFailureResult);
    RUN_TEST(TestResponseFailureResult);
    RUN_TEST(TestTimeoutAndDrainDeadline);
    RUN_TEST(TestLateResponseDrain);
    RUN_TEST(TestBusyBoundary);
    RUN_TEST(TestAnchorModeBoundary);
    RUN_TEST(TestHardwarePortContract);
    RUN_TEST(TestHardwarePortForeignThenActiveResponse);
    RUN_TEST(TestHardwarePortActiveThenForeignResponse);
    RUN_TEST(TestHardwarePortFailureResponse);
    RUN_TEST(TestHardwarePortResponseOverflowPolicy);
    return UNITY_END();
}
