#include "AnchorRangeResponder.h"
#include "TagRangeAggregator.h"

#include <string.h>

namespace
{
class FixedTransport : public IRangeTransport
{
public:
  /**
   * @brief 送信payloadを固定bufferへ記録します。
   *
   * @param destination 送信先MACです。
   * @param payload 送信payloadです。
   * @param length payload長です。
   * @param sequence 割り当てたsequenceの格納先です。
   * @return 容量内ならAccepted、それ以外はFailedです。
   */
  EnRangeSendStartResult Send(const RangeMacAddress &destination,
                              const uint8_t *payload, uint8_t length,
                              uint32_t &sequence) override
  {
    sequence = 0;
    if ( payload == nullptr || length > RANGE_MAX_PAYLOAD_SIZE ||
         m_sentCount >= m_capacity )
    {
      return EnRangeSendStartResult::Failed;
    }
    m_destinations[m_sentCount] = destination;
    m_lengths[m_sentCount] = length;
    memcpy(m_payloads[m_sentCount], payload, length);
    ++m_nextSequence;
    if ( m_nextSequence == 0 )
    {
      ++m_nextSequence;
    }
    sequence = m_nextSequence;
    ++m_sentCount;
    return EnRangeSendStartResult::Accepted;
  }

  /**
   * @brief 固定receive queueの先頭eventを取り出します。
   *
   * @param output eventの格納先です。
   * @return eventが存在する場合はtrueです。
   */
  bool TryPopReceive(EspNowReceiveEvent &output) override
  {
    if ( m_receiveCount == 0 )
    {
      return false;
    }
    output = m_receiveEvents[0];
    for ( uint8_t index = 1; index < m_receiveCount; ++index )
    {
      m_receiveEvents[index - 1] = m_receiveEvents[index];
    }
    --m_receiveCount;
    return true;
  }

  /**
   * @brief 固定send callback queueの先頭eventを取り出します。
   *
   * @param output eventの格納先です。
   * @return このexampleでは常にfalseです。
   */
  bool TryPopSend(EspNowSendEvent &output) override
  {
    (void)output;
    return false;
  }

  /**
   * @brief TAGが受け取る成功reportを固定queueへ追加します。
   *
   * @param source ANCHORのsource MACです。
   * @param roundId 対応するround IDです。
   * @param anchorId 対応するANCHOR IDです。
   * @param receivedAtMs TAG local受信時刻です。
   * @return 追加できた場合はtrueです。
   */
  bool QueueReport(const RangeMacAddress &source, uint32_t roundId,
                   uint16_t anchorId, uint32_t receivedAtMs)
  {
    if ( m_receiveCount >= m_capacity )
    {
      return false;
    }
    RangeReport report = {roundId, anchorId,
                          EnRangeMeasurementStatus::Success, 1250, -42};
    EspNowReceiveEvent &event = m_receiveEvents[m_receiveCount];
    event = {};
    event.m_source = source;
    event.m_length = RANGE_REPORT_SIZE;
    event.m_receivedAtMs = receivedAtMs;
    if ( !EncodeRangeReport(report, event.m_payload, RANGE_REPORT_SIZE) )
    {
      return false;
    }
    ++m_receiveCount;
    return true;
  }

  /**
   * @brief ANCHORが受け取るtokenを固定queueへ追加します。
   *
   * @param source TAGのsource MACです。
   * @param roundId 要求round IDです。
   * @param anchorId 対象ANCHOR IDです。
   * @return 追加できた場合はtrueです。
   */
  bool QueueToken(const RangeMacAddress &source, uint32_t roundId,
                  uint16_t anchorId)
  {
    if ( m_receiveCount >= m_capacity )
    {
      return false;
    }
    const RangeToken token = {roundId, anchorId};
    EspNowReceiveEvent &event = m_receiveEvents[m_receiveCount];
    event = {};
    event.m_source = source;
    event.m_length = RANGE_TOKEN_SIZE;
    if ( !EncodeRangeToken(token, event.m_payload, RANGE_TOKEN_SIZE) )
    {
      return false;
    }
    ++m_receiveCount;
    return true;
  }

  /**
   * @brief 記録済み送信数を返します。
   *
   * @return 固定bufferに記録された送信数です。
   */
  uint8_t SentCount() const { return m_sentCount; }

private:
  static constexpr uint8_t m_capacity = 4;
  EspNowReceiveEvent m_receiveEvents[m_capacity] = {};
  RangeMacAddress m_destinations[m_capacity] = {};
  uint8_t m_payloads[m_capacity][RANGE_MAX_PAYLOAD_SIZE] = {};
  uint8_t m_lengths[m_capacity] = {};
  uint8_t m_receiveCount = 0;
  uint8_t m_sentCount = 0;
  uint32_t m_nextSequence = 0;
};

class FixedUwbRangeProvider : public IUwbRangeProvider
{
public:
  /**
   * @brief 固定値のUWB測距結果を返します。
   *
   * @param tagAddress 測距対象TAG addressです。
   * @param timeoutMs 測距timeoutです。
   * @return 成功した固定距離とRSSIです。
   */
  UwbRangeResult MeasureOnce(const char *tagAddress,
                             uint32_t timeoutMs) override
  {
    (void)tagAddress;
    (void)timeoutMs;
    ++m_measurementCount;
    return {true, 1250, -42};
  }

  /**
   * @brief 実行した測距回数を返します。
   *
   * @return MeasureOnceの呼出回数です。
   */
  uint8_t MeasurementCount() const { return m_measurementCount; }

private:
  uint8_t m_measurementCount = 0;
};

/**
 * @brief 末尾byteを指定したlocal administered unicast MACを作ります。
 *
 * @param last MAC末尾byteです。
 * @return 生成したMACです。
 */
RangeMacAddress MakeMac(uint8_t last)
{
  const RangeMacAddress mac = {{0x02, 0, 0, 0, 0, last}};
  return mac;
}

/**
 * @brief TAG側のdependency injectionと完成sweep取得を実行します。
 *
 * @return 例が完了した場合はtrueです。
 */
bool RunTagComposition()
{
  FixedTransport transport;
  TagRangeAggregator aggregator(transport);
  RangeAggregationConfig config = {};
  config.m_peerCount = 1;
  config.m_timeoutMs = 300;
  config.m_peers[0].m_anchorId = 7;
  config.m_peers[0].m_mac = MakeMac(7);
  aggregator.Configure(config, 100);
  if ( !transport.QueueReport(config.m_peers[0].m_mac, 101, 7, 10) )
  {
    return false;
  }
  aggregator.Update(10);
  RangeSweep sweep = {};
  return aggregator.TryTakeCompletedSweep(sweep) && sweep.m_complete &&
         sweep.m_measurements[0].m_valid;
}

/**
 * @brief ANCHOR側のtransport/provider注入とreport送信を実行します。
 *
 * @return 測距とreport送信が各1回行われた場合はtrueです。
 */
bool RunAnchorComposition()
{
  FixedTransport transport;
  FixedUwbRangeProvider provider;
  AnchorRangeResponder responder(transport, provider);
  RangeAggregationConfig config = {};
  config.m_anchorId = 7;
  config.m_timeoutMs = 300;
  config.m_tagMac = MakeMac(9);
  memcpy(config.m_uwbTagAddress, "TAG00001", 9);
  responder.Configure(config);
  if ( !transport.QueueToken(config.m_tagMac, 1, config.m_anchorId) )
  {
    return false;
  }
  responder.Update();
  return provider.MeasurementCount() == 1 && transport.SentCount() == 1;
}
} // namespace

/**
 * @brief portable coreの最小composition例を実行します。
 *
 * @return TAGとANCHORの例が成功した場合は0です。
 */
int main()
{
  return RunTagComposition() && RunAnchorComposition() ? 0 : 1;
}
