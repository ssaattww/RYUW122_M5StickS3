#pragma once

#include "RangeAggregationTypes.h"

struct EspNowReceiveEvent
{
  RangeMacAddress m_source;
  uint8_t m_length;
  uint8_t m_payload[RANGE_MAX_PAYLOAD_SIZE];
  uint32_t m_receivedAtMs;
};

struct EspNowSendEvent
{
  RangeMacAddress m_destination;
  uint8_t m_status;
  uint32_t m_sequence;
};

enum class EnRangeSendStartResult : uint8_t
{
  Accepted,
  Busy,
  Failed
};

class IRangeTransport
{
public:
  /**
   * @brief 派生transportをinterface経由で安全に破棄します。
   */
  virtual ~IRangeTransport() = default;

  /**
   * @brief 指定peerへpayloadを1回送ります。
   *
   * @param destination 送信先MACです。
   * @param payload 送信payloadです。
   * @param length payload長です。
   * @param sequence 受理した送信に割り当てた非0 sequenceの格納先です。
   * @return 送信の受理、同一宛先処理中、API失敗を区別した開始結果です。
   */
  virtual EnRangeSendStartResult Send(const RangeMacAddress &destination,
                                      const uint8_t *payload, uint8_t length,
                                      uint32_t &sequence) = 0;

  /**
   * @brief 最古のreceive eventを取り出します。
   *
   * @param output eventの格納先です。
   * @return eventが存在した場合はtrueです。
   */
  virtual bool TryPopReceive(EspNowReceiveEvent &output) = 0;

  /**
   * @brief 最古のsend eventを取り出します。
   *
   * @param output eventの格納先です。
   * @return eventが存在した場合はtrueです。
   */
  virtual bool TryPopSend(EspNowSendEvent &output) = 0;
};
