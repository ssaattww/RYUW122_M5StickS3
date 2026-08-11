#pragma once

#include "IRangeTransport.h"

class TagRangeAggregator
{
public:
  /**
   * @brief transportを保持し、停止状態を初期化します。
   *
   * @param transport 送受信に使うESP-NOW transportです。
   */
  explicit TagRangeAggregator(IRangeTransport &transport);

  /**
   * @brief 起動時のTAG peerとtimeoutを固定snapshotとして設定します。
   *
   * @param config 起動時に固定する設定です。
   * @param roundSeed 起動時の非0 random seedです。
   */
  void Configure(const RangeAggregationConfig &config, uint32_t roundSeed);

  /**
   * @brief receive、send result、deadlineを非blockingで処理します。
   *
   * @param nowMs 現在のTAG local millis値です。
   */
  void Update(uint32_t nowMs);

  /**
   * @brief 完成sweepを1回だけ取り出します。
   *
   * @param output 完成sweepの格納先です。
   * @return 取得できた場合はtrueです。
   */
  bool TryTakeCompletedSweep(RangeSweep &output);

private:
  enum class EnState : uint8_t
  {
    Idle,
    SendToken,
    WaitResult,
    Completed
  };

  /**
   * @brief 新しいroundとpeer snapshotを開始します。
   *
   * @param nowMs 開始時刻です。
   */
  void StartRound(uint32_t nowMs);

  /**
   * @brief 現在peerのterminal measurementを保存して次へ進みます。
   *
   * @param status 保存するstatusです。
   * @param distanceMm 成功時の距離です。
   * @param rssi 成功時のRSSIです。
   * @param receivedAtMs reportのTAG受信時刻です。
   * @param nowMs 完了判定時刻です。
   */
  void StoreResult(EnRangeMeasurementStatus status, uint32_t distanceMm,
                   int16_t rssi, uint32_t receivedAtMs, uint32_t nowMs);

  RangeAggregationConfig m_config;
  IRangeTransport &m_transport;
  RangeSweep m_sweep;
  EnState m_state;
  uint8_t m_peerIndex;
  uint32_t m_roundId;
  uint32_t m_deadlineMs;
  uint32_t m_sendSequence;
  bool m_completedAvailable;
  uint32_t m_droppedEventCount;
};
