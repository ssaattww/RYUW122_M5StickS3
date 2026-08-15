#pragma once

#include <cstdint>

#include <esp_now.h>

/**
 * @brief native testのESP timer時刻を設定します。
 *
 * @param timeUs 設定する単調マイクロ秒時刻
 */
void SetEspNowTestTimeUs(uint64_t timeUs);

/**
 * @brief 登録済みESP-NOW受信callbackをnative testから呼び出します。
 *
 * @param info 受信制御情報
 * @param payload 受信payload
 * @param payloadLength 受信payloadサイズ
 * @return callbackが登録済みの場合はtrue、それ以外はfalse
 */
bool InvokeEspNowTestReceive(
    const esp_now_recv_info_t& info,
    const uint8_t* payload,
    int payloadLength);
