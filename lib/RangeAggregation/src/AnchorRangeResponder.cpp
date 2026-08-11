#include "AnchorRangeResponder.h"

AnchorRangeResponder::AnchorRangeResponder(IRangeTransport &transport,
                                           IUwbRangeProvider &uwbController)
    : m_config{}, m_transport(transport), m_uwbController(uwbController),
      m_cachedRoundId(0), m_cachedPayload{}, m_hasCache(false),
      m_sendPending(false), m_droppedEventCount(0), m_sendFailureCount(0)
{
}

void AnchorRangeResponder::Configure(const RangeAggregationConfig &config)
{
  m_config = config;
  m_cachedRoundId = 0;
  m_hasCache = false;
  m_sendPending = false;
}

void AnchorRangeResponder::TrySendPendingReport()
{
  if ( !m_sendPending )
  {
    return;
  }
  uint32_t sendSequence = 0;
  const EnRangeSendStartResult result = m_transport.Send(
      m_config.m_tagMac, m_cachedPayload, RANGE_REPORT_SIZE, sendSequence);
  if ( result == EnRangeSendStartResult::Accepted )
  {
    m_sendPending = false;
  }
  else if ( result == EnRangeSendStartResult::Failed )
  {
    m_sendPending = false;
    ++m_sendFailureCount;
  }
}

void AnchorRangeResponder::Update()
{
  EspNowSendEvent sent = {};
  while ( m_transport.TryPopSend(sent) )
  {
    if ( sent.m_status != 0 )
    {
      ++m_sendFailureCount;
    }
  }

  EspNowReceiveEvent event = {};
  while ( m_transport.TryPopReceive(event) )
  {
    if ( !RangeMacEquals(event.m_source, m_config.m_tagMac) )
    {
      ++m_droppedEventCount;
      continue;
    }
    RangeToken token = {};
    if ( !DecodeRangeToken(event.m_payload, event.m_length, token) ||
         token.m_anchorId != m_config.m_anchorId )
    {
      ++m_droppedEventCount;
      continue;
    }
    if ( m_hasCache && token.m_roundId == m_cachedRoundId )
    {
      m_sendPending = true;
      continue;
    }
    const UwbRangeResult measured = m_uwbController.MeasureOnce(
        m_config.m_uwbTagAddress, m_config.m_timeoutMs);
    RangeReport report = {};
    report.m_roundId = token.m_roundId;
    report.m_anchorId = token.m_anchorId;
    report.m_status = measured.m_success ? EnRangeMeasurementStatus::Success
                                         : EnRangeMeasurementStatus::UwbFailure;
    report.m_distanceMm = measured.m_success ? measured.m_distanceMm : 0;
    report.m_rssi = measured.m_success ? measured.m_rssi : 0;
    if ( EncodeRangeReport(report, m_cachedPayload, RANGE_REPORT_SIZE) )
    {
      m_cachedRoundId = token.m_roundId;
      m_hasCache = true;
      m_sendPending = true;
    }
  }
  TrySendPendingReport();
}
