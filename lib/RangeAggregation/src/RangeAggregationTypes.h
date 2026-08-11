#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t RANGE_MAX_ANCHORS = 8;
constexpr size_t RANGE_MAC_SIZE = 6;
constexpr size_t RANGE_TOKEN_SIZE = 10;
constexpr size_t RANGE_REPORT_SIZE = 18;
constexpr size_t RANGE_MAX_PAYLOAD_SIZE = RANGE_REPORT_SIZE;

enum class EnRangeAggregationMode : uint8_t
{
  Legacy = 0,
  EspNow = 1
};

enum class EnRangeNodeRole : uint8_t
{
  Tag,
  Anchor
};

enum class EnRangeMessageType : uint8_t
{
  RangeToken = 1,
  RangeReport = 2
};

enum class EnRangeMeasurementStatus : uint8_t
{
  Success = 0,
  UwbFailure = 1,
  EspNowSendFailure = 2,
  ReportTimeout = 3,
  InvalidReport = 4
};

enum class EnRangeServiceStatus : uint8_t
{
  Ready,
  MissingConfig,
  ChannelError,
  EspNowInitError,
  PeerError
};

struct RangeMacAddress
{
  uint8_t m_bytes[RANGE_MAC_SIZE];
};

struct RangePeerSetting
{
  uint16_t m_anchorId;
  RangeMacAddress m_mac;
};

struct RangeAggregationConfig
{
  EnRangeAggregationMode m_mode;
  uint8_t m_channel;
  uint8_t m_peerCount;
  uint16_t m_timeoutMs;
  uint16_t m_anchorId;
  char m_uwbTagAddress[9];
  RangeMacAddress m_tagMac;
  RangePeerSetting m_peers[RANGE_MAX_ANCHORS];
};

struct RangeToken
{
  uint32_t m_roundId;
  uint16_t m_anchorId;
};

struct RangeReport
{
  uint32_t m_roundId;
  uint16_t m_anchorId;
  EnRangeMeasurementStatus m_status;
  uint32_t m_distanceMm;
  int16_t m_rssi;
};

struct RangeMeasurement
{
  uint16_t m_anchorId;
  EnRangeMeasurementStatus m_status;
  uint32_t m_distanceMm;
  int16_t m_rssi;
  uint32_t m_receivedAtMs;
  bool m_valid;
};

struct RangeSweep
{
  uint32_t m_roundId;
  uint8_t m_measurementCount;
  RangeMeasurement m_measurements[RANGE_MAX_ANCHORS];
  uint32_t m_completedAtMs;
  bool m_complete;
};

static_assert(sizeof(uint8_t) == 1, "uint8_t must be one byte");
static_assert(sizeof(uint16_t) == 2, "uint16_t must be two bytes");
static_assert(sizeof(int16_t) == 2, "int16_t must be two bytes");
static_assert(sizeof(uint32_t) == 4, "uint32_t must be four bytes");
static_assert(RANGE_TOKEN_SIZE == 10, "RangeToken wire size changed");
static_assert(RANGE_REPORT_SIZE == 18, "RangeReport wire size changed");

/**
 * @brief 2個のMAC addressがbyte単位で一致するか判定します。
 *
 * @param left 比較する左辺のMAC addressです。
 * @param right 比較する右辺のMAC addressです。
 * @return 全byteが一致する場合はtrueです。
 */
bool RangeMacEquals(const RangeMacAddress &left, const RangeMacAddress &right);

/**
 * @brief MAC addressが未設定を表す全0か判定します。
 *
 * @param mac 判定するMAC addressです。
 * @return 全byteが0の場合はtrueです。
 */
bool RangeMacIsUnset(const RangeMacAddress &mac);

/**
 * @brief 文字列表現のunicast MAC addressを固定6 byteへ変換します。
 *
 * @param text colon区切りのMAC address文字列です。
 * @param output 変換結果を受け取るMAC addressです。
 * @return 形式とunicast条件が正しい場合はtrueです。
 */
bool ParseRangeMacAddress(const char *text, RangeMacAddress &output);

/**
 * @brief RangeTokenを10 byteのlittle-endian payloadへencodeします。
 *
 * @param token encodeするtokenです。
 * @param output 10 byteの出力先です。
 * @param outputSize 出力先のbyte数です。
 * @return 入力と出力sizeが妥当な場合はtrueです。
 */
bool EncodeRangeToken(const RangeToken &token, uint8_t *output,
                      size_t outputSize);

/**
 * @brief 10 byteのlittle-endian payloadからRangeTokenをdecodeします。
 *
 * @param data decodeするpayloadです。
 * @param dataSize payloadのbyte数です。
 * @param output decode結果を受け取るtokenです。
 * @return protocolとfieldが妥当な場合はtrueです。
 */
bool DecodeRangeToken(const uint8_t *data, size_t dataSize, RangeToken &output);

/**
 * @brief RangeReportを18 byteのlittle-endian payloadへencodeします。
 *
 * @param report encodeするreportです。
 * @param output 18 byteの出力先です。
 * @param outputSize 出力先のbyte数です。
 * @return 入力と出力sizeが妥当な場合はtrueです。
 */
bool EncodeRangeReport(const RangeReport &report, uint8_t *output,
                       size_t outputSize);

/**
 * @brief 18 byteのlittle-endian payloadからRangeReportをdecodeします。
 *
 * @param data decodeするpayloadです。
 * @param dataSize payloadのbyte数です。
 * @param output decode結果を受け取るreportです。
 * @return protocolとfield invariantが妥当な場合はtrueです。
 */
bool DecodeRangeReport(const uint8_t *data, size_t dataSize,
                       RangeReport &output);
