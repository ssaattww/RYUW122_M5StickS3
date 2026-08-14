#pragma once

#include <Arduino.h>
#include <RYUW122.h>

#include "ConfigRuntime.h"

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
};

/**
 * @brief RYUW122のUART初期化と端末設定を管理します。
 */
class Ryuw122Controller
{
public:
    /**
     * @brief RYUW122制御クラスを生成します。
     * G7を送信、G1を受信として115200bpsでUARTを使用します。
     *
     * @param serial RYUW122との通信に使用するHardwareSerial
     * @param configRuntime 動作モードとノードIDを保持する実行時設定
     */
    Ryuw122Controller(
        HardwareSerial& serial,
        ConfigRuntime& configRuntime);

    /**
     * @brief RYUW122との通信を開始し、モード、ネットワークID、アドレスを設定します。
     * 現在値が目的の値と一致している項目は書き換えません。
     *
     * @return 初期化結果
     */
    EnRyuw122InitResult Begin();

    /**
     * @brief RYUW122から受信したデータを処理します。
     */
    void Update();

    /**
     * @brief RYUW122が利用可能な状態か確認します。
     *
     * @return 初期化済みの場合はtrue、それ以外はfalse
     */
    bool IsReady() const;

    /**
     * @brief 最後の初期化結果を取得します。
     *
     * @return 最後の初期化結果
     */
    EnRyuw122InitResult GetLastResult() const;

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
     * @brief RYUW122の動作モードを確認し、必要な場合だけ変更します。
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

    ConfigRuntime& m_configRuntime;
    RYUW122 m_ryuw122;
    bool m_isReady = false;
    EnRyuw122InitResult m_lastResult = EnRyuw122InitResult::SerialBeginFailed;
};
