#pragma once

#include <stdint.h>

struct UwbRangeResult
{
  bool m_success;
  uint32_t m_distanceMm;
  int16_t m_rssi;
};

class IUwbRangeProvider
{
public:
  /**
   * @brief 派生UWB providerをinterface経由で安全に破棄します。
   */
  virtual ~IUwbRangeProvider() = default;

  /**
   * @brief 指定TAGへblocking UWB測距を1回行います。
   *
   * @param tagAddress 測距対象TAG addressです。
   * @param timeoutMs 測距timeoutです。
   * @return 測距結果です。
   */
  virtual UwbRangeResult MeasureOnce(const char *tagAddress,
                                     uint32_t timeoutMs) = 0;
};
