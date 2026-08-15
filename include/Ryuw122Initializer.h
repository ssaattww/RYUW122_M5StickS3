#pragma once

#include <cstdint>

class ConfigRuntime;

/**
 * @brief RYUW122の初期化結果を表します。
 */
enum class EnRyuw122InitResult : uint8_t
{
    Ok,
    SerialBeginFailed,
    CommunicationFailed,
    ModeReadFailed,
    ModeWriteFailed,
    NetworkIdReadFailed,
    NetworkIdWriteFailed,
    AddressReadFailed,
    AddressWriteFailed,
    TagResponseWriteFailed,
};

/**
 * @brief RYUW122 portが扱う動作モードを表します。
 */
enum class EnRyuw122PortMode : uint8_t
{
    Tag,
    Anchor,
    Unknown,
};

/**
 * @brief RYUW122測距処理の内部診断理由を表します。
 */
enum class EnRyuw122RangingReason : uint8_t
{
    Success,
    ErrorResponse,
    ParseError,
    StartFailure,
    Timeout,
};

/**
 * @brief RYUW122 portが受信した測距応答を保持します。
 */
struct Ryuw122PortResponse
{
    char tagAddress[9] = {};
    bool isSuccess = false;
    int32_t distanceCm = 0;
    int16_t uwbRssi = 0;
    EnRyuw122RangingReason reason = EnRyuw122RangingReason::ParseError;
    int32_t diagnosticCode = 0;
    bool isAcknowledgement = false;
};

/**
 * @brief RYUW122の初期化と非同期UART入出力を抽象化します。
 */
class IRyuw122Port
{
public:
    /**
     * @brief RYUW122 portを破棄します。
     */
    virtual ~IRyuw122Port() = default;

    /**
     * @brief RYUW122用UARTを初期化します。
     *
     * @return 初期化できた場合はtrue、それ以外はfalse
     */
    virtual bool Begin() = 0;

    /**
     * @brief RYUW122のNRSTを使用して通信状態を復旧します。
     */
    virtual void Recover() = 0;

    /**
     * @brief RYUW122とのAT通信を確認します。
     *
     * @return 応答を確認できた場合はtrue、それ以外はfalse
     */
    virtual bool Test() = 0;

    /**
     * @brief 現在のRYUW122動作モードを取得します。
     *
     * @return 現在の動作モード
     */
    virtual EnRyuw122PortMode GetMode() = 0;

    /**
     * @brief RYUW122動作モードを設定します。
     *
     * @param mode 設定する動作モード
     * @return 設定できた場合はtrue、それ以外はfalse
     */
    virtual bool SetMode(EnRyuw122PortMode mode) = 0;

    /**
     * @brief 現在のネットワークIDを取得します。
     *
     * @param networkId 取得した8文字のIDを格納する9バイト以上の領域
     * @return 取得できた場合はtrue、それ以外はfalse
     */
    virtual bool GetNetworkId(char* networkId) = 0;

    /**
     * @brief ネットワークIDを設定します。
     *
     * @param networkId 設定する8文字のID
     * @return 設定できた場合はtrue、それ以外はfalse
     */
    virtual bool SetNetworkId(const char* networkId) = 0;

    /**
     * @brief 現在の端末アドレスを取得します。
     *
     * @param address 取得した8文字のアドレスを格納する9バイト以上の領域
     * @return 取得できた場合はtrue、それ以外はfalse
     */
    virtual bool GetAddress(char* address) = 0;

    /**
     * @brief 端末アドレスを設定します。
     *
     * @param address 設定する8文字のアドレス
     * @return 設定できた場合はtrue、それ以外はfalse
     */
    virtual bool SetAddress(const char* address) = 0;

    /**
     * @brief TAG動作時にANCHORへ返すpayloadを登録します。
     *
     * @param length payload長
     * @param data 登録するpayload
     * @return 登録できた場合はtrue、それ以外はfalse
     */
    virtual bool SetTagResponse(uint8_t length, const char* data) = 0;

    /**
     * @brief 指定TAGへの測距コマンドを待機せず送信します。
     *
     * @param tagAddress 測距対象の8文字TAGアドレス
     * @return コマンドをUARTへ投入できた場合はtrue、それ以外はfalse
     */
    virtual bool StartRanging(const char* tagAddress) = 0;

    /**
     * @brief UARTから到着済みのRYUW122応答を処理します。
     */
    virtual void Update() = 0;

    /**
     * @brief 処理済みの測距応答を1件取得します。
     *
     * @param response 取得した応答の格納先
     * @return 応答を取得した場合はtrue、それ以外はfalse
     */
    virtual bool TryTakeResponse(Ryuw122PortResponse& response) = 0;
};

/**
 * @brief RYUW122のUART開始、NRST復旧、AT疎通、設定を一括管理します。
 */
class Ryuw122Initializer
{
public:
    /**
     * @brief RYUW122初期化処理を生成します。
     *
     * @param port 初期化対象のRYUW122 port
     * @param configRuntime 動作モードとノードIDを保持する実行時設定
     */
    Ryuw122Initializer(
        IRyuw122Port& port,
        ConfigRuntime& configRuntime);

    /**
     * @brief UART開始後にNRST復旧、AT疎通、各設定を順番に実行します。
     * 最初のAT疎通だけ失敗した場合はNRST復旧とAT疎通を1回再試行します。
     *
     * @return 初期化結果
     */
    EnRyuw122InitResult Begin();

    /**
     * @brief 初期化結果を表示用文字列へ変換します。
     *
     * @param result 変換する初期化結果
     * @return 初期化結果を表す固定文字列
     */
    static const char* GetResultName(EnRyuw122InitResult result);

private:
    /**
     * @brief 実行時設定からRYUW122へ設定する8文字のアドレスを生成します。
     *
     * @param address 生成したアドレスを格納する9バイト以上の領域
     */
    void BuildAddress(char* address) const;

    /**
     * @brief RYUW122の動作モードを確認し、変更成功時だけ2秒待機します。
     *
     * @return モード設定結果
     */
    EnRyuw122InitResult ConfigureMode();

    /**
     * @brief RYUW122のネットワークIDを確認し、必要な場合だけ変更します。
     *
     * @return ネットワークID設定結果
     */
    EnRyuw122InitResult ConfigureNetworkId();

    /**
     * @brief RYUW122のアドレスを確認し、必要な場合だけ変更します。
     *
     * @return アドレス設定結果
     */
    EnRyuw122InitResult ConfigureAddress();

    /**
     * @brief TAG動作時の測距応答payloadを登録します。
     *
     * @return payload登録結果
     */
    EnRyuw122InitResult ConfigureTagResponse();

    IRyuw122Port& m_port;
    ConfigRuntime& m_configRuntime;
};
