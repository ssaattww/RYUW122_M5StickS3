#include "RangeAggregationTypes.h"

#include <string.h>

namespace
{
constexpr uint8_t MAGIC_0 = 0x52;
constexpr uint8_t MAGIC_1 = 0x47;
constexpr uint8_t PROTOCOL_VERSION = 1;

/**
 * @brief 16 bit整数をlittle-endianで書き込みます。
 *
 * @param output 書込み先です。
 * @param value 書き込む値です。
 */
void WriteUint16(uint8_t *output, uint16_t value)
{
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
}

/**
 * @brief 32 bit整数をlittle-endianで書き込みます。
 *
 * @param output 書込み先です。
 * @param value 書き込む値です。
 */
void WriteUint32(uint8_t *output, uint32_t value)
{
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

/**
 * @brief little-endianの16 bit整数を読み取ります。
 *
 * @param input 読取り元です。
 * @return 復元した値です。
 */
uint16_t ReadUint16(const uint8_t *input)
{
  return static_cast<uint16_t>(input[0]) | static_cast<uint16_t>(input[1]) << 8;
}

/**
 * @brief little-endianの32 bit整数を読み取ります。
 *
 * @param input 読取り元です。
 * @return 復元した値です。
 */
uint32_t ReadUint32(const uint8_t *input)
{
  return static_cast<uint32_t>(input[0]) |
         static_cast<uint32_t>(input[1]) << 8 |
         static_cast<uint32_t>(input[2]) << 16 |
         static_cast<uint32_t>(input[3]) << 24;
}

/**
 * @brief payload共通headerと固定長を検証します。
 *
 * @param data payloadです。
 * @param dataSize payload長です。
 * @param expectedSize 期待する固定長です。
 * @param expectedType 期待するmessage typeです。
 * @return 共通headerが契約どおりならtrueです。
 */
bool HasCommonHeader(const uint8_t *data, size_t dataSize, size_t expectedSize,
                     EnRangeMessageType expectedType)
{
  return data != nullptr && dataSize == expectedSize && data[0] == MAGIC_0 &&
         data[1] == MAGIC_1 && data[2] == PROTOCOL_VERSION &&
         data[3] == static_cast<uint8_t>(expectedType);
}

/**
 * @brief 16進ASCII 1文字を数値へ変換します。
 *
 * @param value 変換する文字です。
 * @return 0から15、または不正時に-1です。
 */
int HexValue(char value)
{
  if ( value >= '0' && value <= '9' )
  {
    return value - '0';
  }

  if ( value >= 'a' && value <= 'f' )
  {
    return value - 'a' + 10;
  }

  if ( value >= 'A' && value <= 'F' )
  {
    return value - 'A' + 10;
  }

  return -1;
}
} // namespace

bool RangeMacEquals(const RangeMacAddress &left, const RangeMacAddress &right)
{
  return memcmp(left.m_bytes, right.m_bytes, RANGE_MAC_SIZE) == 0;
}

bool RangeMacIsUnset(const RangeMacAddress &mac)
{
  static const uint8_t unset[RANGE_MAC_SIZE] = {};

  return memcmp(mac.m_bytes, unset, RANGE_MAC_SIZE) == 0;
}

bool ParseRangeMacAddress(const char *text, RangeMacAddress &output)
{
  if ( text == nullptr || strlen(text) != 17 )
  {
    return false;
  }

  RangeMacAddress candidate = {};

  for ( size_t index = 0; index < RANGE_MAC_SIZE; ++index )
  {
    const size_t offset = index * 3;
    const int high = HexValue(text[offset]);
    const int low = HexValue(text[offset + 1]);

    if ( high < 0 || low < 0 )
    {
      return false;
    }

    if ( index + 1 < RANGE_MAC_SIZE && text[offset + 2] != ':' )
    {
      return false;
    }

    candidate.m_bytes[index] = static_cast<uint8_t>((high << 4) | low);
  }

  if ( (candidate.m_bytes[0] & 0x01) != 0 || RangeMacIsUnset(candidate) )
  {
    return false;
  }

  output = candidate;
  return true;
}

bool EncodeRangeToken(const RangeToken &token, uint8_t *output,
                      size_t outputSize)
{
  if ( output == nullptr || outputSize != RANGE_TOKEN_SIZE ||
       token.m_roundId == 0 || token.m_anchorId == 0 )
  {
    return false;
  }

  output[0] = MAGIC_0;
  output[1] = MAGIC_1;
  output[2] = PROTOCOL_VERSION;
  output[3] = static_cast<uint8_t>(EnRangeMessageType::RangeToken);
  WriteUint32(output + 4, token.m_roundId);
  WriteUint16(output + 8, token.m_anchorId);
  return true;
}

bool DecodeRangeToken(const uint8_t *data, size_t dataSize, RangeToken &output)
{
  if ( !HasCommonHeader(data, dataSize, RANGE_TOKEN_SIZE,
                        EnRangeMessageType::RangeToken) )
  {
    return false;
  }

  RangeToken candidate = {};
  candidate.m_roundId = ReadUint32(data + 4);
  candidate.m_anchorId = ReadUint16(data + 8);

  if ( candidate.m_roundId == 0 || candidate.m_anchorId == 0 )
  {
    return false;
  }

  output = candidate;
  return true;
}

bool EncodeRangeReport(const RangeReport &report, uint8_t *output,
                       size_t outputSize)
{
  const bool success = report.m_status == EnRangeMeasurementStatus::Success;
  const bool failure = report.m_status == EnRangeMeasurementStatus::UwbFailure;

  if ( output == nullptr || outputSize != RANGE_REPORT_SIZE ||
       report.m_roundId == 0 || report.m_anchorId == 0 ||
       (!success && !failure) ||
       (failure && (report.m_distanceMm != 0 || report.m_rssi != 0)) )
  {
    return false;
  }

  output[0] = MAGIC_0;
  output[1] = MAGIC_1;
  output[2] = PROTOCOL_VERSION;
  output[3] = static_cast<uint8_t>(EnRangeMessageType::RangeReport);
  WriteUint32(output + 4, report.m_roundId);
  WriteUint16(output + 8, report.m_anchorId);
  output[10] = static_cast<uint8_t>(report.m_status);
  output[11] = 0;
  WriteUint32(output + 12, report.m_distanceMm);
  WriteUint16(output + 16, static_cast<uint16_t>(report.m_rssi));
  return true;
}

bool DecodeRangeReport(const uint8_t *data, size_t dataSize,
                       RangeReport &output)
{
  if ( !HasCommonHeader(data, dataSize, RANGE_REPORT_SIZE,
                        EnRangeMessageType::RangeReport) ||
       data[11] != 0 )
  {
    return false;
  }

  RangeReport candidate = {};
  candidate.m_roundId = ReadUint32(data + 4);
  candidate.m_anchorId = ReadUint16(data + 8);
  candidate.m_status = static_cast<EnRangeMeasurementStatus>(data[10]);
  candidate.m_distanceMm = ReadUint32(data + 12);
  candidate.m_rssi = static_cast<int16_t>(ReadUint16(data + 16));

  if ( candidate.m_roundId == 0 || candidate.m_anchorId == 0 ||
       (candidate.m_status != EnRangeMeasurementStatus::Success &&
        candidate.m_status != EnRangeMeasurementStatus::UwbFailure) ||
       (candidate.m_status == EnRangeMeasurementStatus::UwbFailure &&
        (candidate.m_distanceMm != 0 || candidate.m_rssi != 0)) )
  {
    return false;
  }

  output = candidate;
  return true;
}
