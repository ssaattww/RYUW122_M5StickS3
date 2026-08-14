#pragma once

#include <cstdint>

/**
 * @brief test用RYUW122初期化結果を表します。
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
 * @brief test用RYUW122表示名を提供します。
 */
class Ryuw122Controller
{
public:
    /**
     * @brief 初期化結果の表示名を取得します。
     *
     * @param result 初期化結果
     * @return 初期化結果の表示名
     */
    static const char* GetResultName(EnRyuw122InitResult result)
    {
        return result == EnRyuw122InitResult::Ok
            ? "Ok"
            : "CommunicationFailed";
    }
};
