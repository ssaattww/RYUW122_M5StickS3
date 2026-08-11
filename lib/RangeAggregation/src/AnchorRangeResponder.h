#pragma once

#include "IRangeTransport.h"
#include "IUwbRangeProvider.h"

class AnchorRangeResponder
{
public:
  /**
   * @brief transportとUWB controllerを保持します。
   *
   * @param transport report送信に使うtransportです。
   * @param uwbController 1回測距を行うcontrollerです。
   */
  AnchorRangeResponder(IRangeTransport &transport,
                       IUwbRangeProvider &uwbController);

  /**
   * @brief 起動時ANCHOR設定を固定snapshotとして設定します。
   *
   * @param config self ID、TAG MAC、UWB addressの設定です。
   */
  void Configure(const RangeAggregationConfig &config);

  /**
   * @brief valid tokenを処理し、測距またはcached report送信を行います。
   */
  void Update();

private:
  /**
   * @brief 最新cached reportの保留要求を1回だけ送信開始します。
   */
  void TrySendPendingReport();

  RangeAggregationConfig m_config;
  IRangeTransport &m_transport;
  IUwbRangeProvider &m_uwbController;
  uint32_t m_cachedRoundId;
  uint8_t m_cachedPayload[RANGE_REPORT_SIZE];
  bool m_hasCache;
  bool m_sendPending;
  uint32_t m_droppedEventCount;
  uint32_t m_sendFailureCount;
};
