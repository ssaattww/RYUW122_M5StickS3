#include "TagRangeAggregator.h"

#include <string.h>

TagRangeAggregator::TagRangeAggregator(IRangeTransport &transport)
    : m_config{}, m_transport(transport), m_sweep{},
      m_state(EnState::Completed), m_peerIndex(0), m_roundId(1),
      m_deadlineMs(0), m_sendSequence(0), m_completedAvailable(false),
      m_droppedEventCount(0)
{
}

void TagRangeAggregator::Configure(const RangeAggregationConfig &config,
                                   uint32_t roundSeed)
{
  m_config = config;
  m_roundId = roundSeed == 0 ? 1 : roundSeed;
  m_state = EnState::Idle;
  m_sendSequence = 0;
  m_completedAvailable = false;
}

void TagRangeAggregator::StartRound(uint32_t nowMs)
{
  memset(&m_sweep, 0, sizeof(m_sweep));
  ++m_roundId;
  if ( m_roundId == 0 )
  {
    ++m_roundId;
  }
  m_sweep.m_roundId = m_roundId;
  m_sweep.m_measurementCount = m_config.m_peerCount;
  for ( uint8_t index = 0; index < m_config.m_peerCount; ++index )
  {
    m_sweep.m_measurements[index].m_anchorId =
        m_config.m_peers[index].m_anchorId;
  }
  m_peerIndex = 0;
  m_deadlineMs = nowMs + m_config.m_timeoutMs + 100;
  m_state = EnState::SendToken;
}

void TagRangeAggregator::StoreResult(EnRangeMeasurementStatus status,
                                     uint32_t distanceMm, int16_t rssi,
                                     uint32_t receivedAtMs, uint32_t nowMs)
{
  RangeMeasurement &value = m_sweep.m_measurements[m_peerIndex];
  value.m_status = status;
  value.m_valid = status == EnRangeMeasurementStatus::Success;
  value.m_distanceMm = value.m_valid ? distanceMm : 0;
  value.m_rssi = value.m_valid ? rssi : 0;
  value.m_receivedAtMs = receivedAtMs;
  ++m_peerIndex;
  if ( m_peerIndex >= m_sweep.m_measurementCount )
  {
    m_sweep.m_completedAtMs = nowMs;
    m_sweep.m_complete = true;
    m_completedAvailable = true;
    m_state = EnState::Completed;
  }
  else
  {
    m_state = EnState::SendToken;
  }
}

void TagRangeAggregator::Update(uint32_t nowMs)
{
  if ( m_state == EnState::Completed )
  {
    return;
  }
  if ( m_state == EnState::Idle )
  {
    StartRound(nowMs);
  }

  if ( m_state == EnState::SendToken )
  {
    RangeToken token = {m_sweep.m_roundId,
                        m_config.m_peers[m_peerIndex].m_anchorId};
    uint8_t payload[RANGE_TOKEN_SIZE] = {};
    uint32_t sendSequence = 0;
    if ( !EncodeRangeToken(token, payload, sizeof(payload)) )
    {
      StoreResult(EnRangeMeasurementStatus::EspNowSendFailure, 0, 0, 0, nowMs);
      return;
    }
    const EnRangeSendStartResult sendResult =
        m_transport.Send(m_config.m_peers[m_peerIndex].m_mac, payload,
                         sizeof(payload), sendSequence);
    if ( sendResult == EnRangeSendStartResult::Busy )
    {
      if ( static_cast<int32_t>(nowMs - m_deadlineMs) >= 0 )
      {
        StoreResult(EnRangeMeasurementStatus::ReportTimeout, 0, 0, 0, nowMs);
      }
      return;
    }
    if ( sendResult == EnRangeSendStartResult::Failed )
    {
      StoreResult(EnRangeMeasurementStatus::EspNowSendFailure, 0, 0, 0, nowMs);
      return;
    }
    m_sendSequence = sendSequence;
    m_deadlineMs = nowMs + m_config.m_timeoutMs + 100;
    m_state = EnState::WaitResult;
  }

  if ( m_state != EnState::WaitResult )
  {
    return;
  }
  EspNowReceiveEvent receive = {};
  while ( m_transport.TryPopReceive(receive) )
  {
    const RangePeerSetting &peer = m_config.m_peers[m_peerIndex];
    if ( !RangeMacEquals(receive.m_source, peer.m_mac) )
    {
      ++m_droppedEventCount;
      continue;
    }
    RangeReport report = {};
    if ( !DecodeRangeReport(receive.m_payload, receive.m_length, report) )
    {
      StoreResult(EnRangeMeasurementStatus::InvalidReport, 0, 0,
                  receive.m_receivedAtMs, nowMs);
      return;
    }
    if ( report.m_roundId != m_sweep.m_roundId ||
         report.m_anchorId != peer.m_anchorId )
    {
      ++m_droppedEventCount;
      continue;
    }
    StoreResult(report.m_status, report.m_distanceMm, report.m_rssi,
                receive.m_receivedAtMs, nowMs);
    return;
  }
  EspNowSendEvent sent = {};
  while ( m_transport.TryPopSend(sent) )
  {
    if ( sent.m_sequence != m_sendSequence )
    {
      ++m_droppedEventCount;
      continue;
    }
    if ( RangeMacEquals(sent.m_destination,
                        m_config.m_peers[m_peerIndex].m_mac) &&
         sent.m_status != 0 )
    {
      StoreResult(EnRangeMeasurementStatus::EspNowSendFailure, 0, 0, 0, nowMs);
      return;
    }
    if ( !RangeMacEquals(sent.m_destination,
                         m_config.m_peers[m_peerIndex].m_mac) )
    {
      ++m_droppedEventCount;
    }
  }
  if ( static_cast<int32_t>(nowMs - m_deadlineMs) >= 0 )
  {
    StoreResult(EnRangeMeasurementStatus::ReportTimeout, 0, 0, 0, nowMs);
  }
}

bool TagRangeAggregator::TryTakeCompletedSweep(RangeSweep &output)
{
  if ( !m_completedAvailable )
  {
    return false;
  }
  output = m_sweep;
  m_completedAvailable = false;
  m_state = EnState::Idle;
  return true;
}
