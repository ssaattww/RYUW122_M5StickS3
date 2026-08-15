#pragma once

#include "Ryuw122Initializer.h"

#include <cstdint>

class ConfigRuntime;
class HardwareSerial;

/**
 * @brief 非同期測距の完了状態を表します。
 */
enum class EnRyuw122RangingStatus : uint8_t
{
    Success,
    Failed,
    TimedOut,
};

/**
 * @brief アプリケーションへ公開する非同期測距結果を保持します。
 */
struct Ryuw122RangingResult
{
    char tagAddress[9] = {};
    EnRyuw122RangingStatus status = EnRyuw122RangingStatus::Failed;
    uint32_t distanceMm = 0;
    int16_t uwbRssi = 0;
    uint32_t startedAtUs = 0;
    uint32_t completedAtUs = 0;
};

/**
 * @brief 非同期測距で使用するマイクロ秒時刻取得関数を表します。
 */
using Ryuw122TimeProvider = uint32_t (*)();

/**
 * @brief 初期化済みRYUW122の非同期測距を管理します。
 */
class Ryuw122Controller
{
public:
    /**
     * @brief 実機用RYUW122制御クラスを生成します。
     * G7を送信、G1を受信として115200bpsでUARTを使用します。
     *
     * @param serial RYUW122との通信に使用するHardwareSerial
     * @param configRuntime 動作モードとノードIDを保持する実行時設定
     * @param timeProvider マイクロ秒時刻を返す関数。nullptrの場合はmicrosを使用
     */
    Ryuw122Controller(
        HardwareSerial& serial,
        ConfigRuntime& configRuntime,
        Ryuw122TimeProvider timeProvider = nullptr);

    /**
     * @brief 差し替え可能なportを使用するRYUW122制御クラスを生成します。
     *
     * @param port 初期化と非同期通信に使用するport
     * @param configRuntime 動作モードとノードIDを保持する実行時設定
     * @param timeProvider マイクロ秒時刻を返す関数
     */
    Ryuw122Controller(
        IRyuw122Port& port,
        ConfigRuntime& configRuntime,
        Ryuw122TimeProvider timeProvider);

    /**
     * @brief 所有している実機用portを破棄します。
     */
    ~Ryuw122Controller();

    /**
     * @brief port所有権の重複を防ぐためコピー生成を禁止します。
     *
     * @param other コピー元
     */
    Ryuw122Controller(const Ryuw122Controller& other) = delete;

    /**
     * @brief port所有権の重複を防ぐためコピー代入を禁止します。
     *
     * @param other コピー元
     * @return 代入後の自身
     */
    Ryuw122Controller& operator=(const Ryuw122Controller& other) = delete;

    /**
     * @brief InitializerでRYUW122を初期化し、測距状態を開始可能にします。
     *
     * @return 初期化結果
     */
    EnRyuw122InitResult Begin();

    /**
     * @brief RYUW122から受信したデータ、timeout、遅延応答排出を処理します。
     */
    void Update();

    /**
     * @brief ANCHORから指定TAGへの非同期測距を開始します。
     * 結果未取得、測距中、遅延応答排出中は新しい測距を受け付けません。
     *
     * @param tagAddress 測距対象の8文字TAGアドレス
     * @return 測距要求を受け付けた場合はtrue、それ以外はfalse
     */
    bool StartRanging(const char* tagAddress);

    /**
     * @brief 完了した測距結果を1件取得します。
     *
     * @param result 取得した測距結果の格納先
     * @return 結果を取得した場合はtrue、それ以外はfalse
     */
    bool TryTakeResult(Ryuw122RangingResult& result);

    /**
     * @brief 測距中または遅延応答排出中か確認します。
     *
     * @return 新しい測距を開始できない場合はtrue、それ以外はfalse
     */
    bool IsBusy() const;

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
     * @brief 非同期測距の内部状態を表します。
     */
    enum class EnRangingState : uint8_t
    {
        Idle,
        WaitingForResponse,
        DrainingLateResponse,
    };

    /**
     * @brief 現在時刻で測距結果を確定します。
     *
     * @param status 確定する完了状態
     * @param distanceMm 距離。失敗時は0
     * @param uwbRssi UWB RSSI。失敗時は0
     * @param completedAtUs 測距完了時刻
     */
    void CompleteRanging(
        EnRyuw122RangingStatus status,
        uint32_t distanceMm,
        int16_t uwbRssi,
        uint32_t completedAtUs);

    /**
     * @brief timeout後の遅延応答排出状態へ移ります。
     *
     * @param nowUs timeoutを検出した時刻
     */
    void BeginLateResponseDrain(uint32_t nowUs);

    ConfigRuntime& m_configRuntime;
    IRyuw122Port* m_port;
    Ryuw122Initializer m_initializer;
    Ryuw122TimeProvider m_timeProvider;
    bool m_ownsPort;
    bool m_isReady = false;
    bool m_hasResult = false;
    EnRyuw122InitResult m_lastResult =
        EnRyuw122InitResult::SerialBeginFailed;
    EnRangingState m_rangingState = EnRangingState::Idle;
    Ryuw122RangingResult m_result{};
    char m_activeTagAddress[9] = {};
    char m_drainTagAddress[9] = {};
    uint32_t m_rangingStartedAtUs = 0;
    uint32_t m_drainStartedAtUs = 0;

    static constexpr uint32_t m_rangingTimeoutUs = 300000U;
    static constexpr uint32_t m_lateResponseDrainTimeoutUs = 300000U;
};
