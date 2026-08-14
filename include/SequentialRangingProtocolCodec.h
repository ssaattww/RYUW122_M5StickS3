#pragma once

#include <cstddef>
#include <cstdint>

#include "NtpTimeProtocolCodec.h"

/**
 * @brief 逐次測距packetの種別を表します。
 */
enum class EnSequentialRangingPacketType : uint8_t
{
    RangeControl = 5,
    RangeMeasurement = 6,
    RangeMeasurementForward = 7,
    RangeRoundComplete = 8,
};

/**
 * @brief 逐次測距結果の状態を表します。
 */
enum class EnRangeResultStatus : uint8_t
{
    Success = 0,
    Failed = 1,
    TimedOut = 2,
    Unreachable = 3,
};

/**
 * @brief 測距対象ノードの識別情報を表します。
 */
struct RangingNodeIdentity
{
    uint8_t nodeId = 0;
    uint8_t macAddress[6]{};
    char uwbAddress[9]{};
};

/**
 * @brief 測距制御packetへ格納する値を表します。
 */
struct RangeControlData
{
    uint32_t roundId = 0;
    uint16_t pairSequence = 0;
    uint8_t masterTagId = 0;
    uint8_t masterMac[6]{};
    uint8_t anchorCount = 0;
    uint8_t tagCount = 0;
    uint8_t anchorIndex = 0;
    uint8_t tagIndex = 0;
    uint8_t anchorIds[8]{};
    uint8_t tagIds[8]{};
};

/**
 * @brief 逐次測距結果packetへ格納する値を表します。
 */
struct RangeMeasurementData
{
    uint32_t roundId = 0;
    uint16_t pairSequence = 0;
    uint8_t masterTagId = 0;
    uint8_t masterMac[6]{};
    uint8_t anchorCount = 0;
    uint8_t tagCount = 0;
    uint8_t anchorIndex = 0;
    uint8_t tagIndex = 0;
    RangingNodeIdentity anchor{};
    RangingNodeIdentity tag{};
    EnRangeResultStatus status = EnRangeResultStatus::Failed;
    uint32_t distanceMm = 0;
    int16_t uwbRssi = 0;
    uint32_t commandReceivedUs = 0;
    uint32_t rangingStartedUs = 0;
    uint32_t rangingCompletedUs = 0;
    int8_t espNowRssi = 0;
    uint64_t commandReceivedMasterTimeUs = 0;
    uint64_t rangingStartedMasterTimeUs = 0;
    uint64_t rangingCompletedMasterTimeUs = 0;
    uint32_t synchronizationRoundTripUs = 0;
    uint64_t synchronizationAgeUs = 0;
    EnTimeQuality timeQuality = EnTimeQuality::Unsynchronized;
    bool isLastMeasurement = false;
};

/**
 * @brief ラウンド完了packetへ格納する値を表します。
 */
struct RangeRoundCompleteData
{
    uint32_t roundId = 0;
    uint32_t nextRoundId = 0;
    uint8_t masterTagId = 0;
    uint8_t masterMac[6]{};
    uint64_t startedMasterTimeUs = 0;
    uint64_t completedMasterTimeUs = 0;
    uint8_t anchorCount = 0;
    uint8_t tagCount = 0;
    uint8_t expectedMeasurementCount = 0;
    uint8_t receivedMeasurementCount = 0;
    uint64_t missingMeasurementBits = 0;
    bool anchorListTruncated = false;
    bool tagListTruncated = false;
    bool timedOut = false;
};

#pragma pack(push, 1)
/**
 * @brief 逐次測距packetの固定共通headerを表します。
 */
struct SequentialRangingPacketHeader
{
    uint16_t magic;
    uint8_t version;
    uint8_t packetType;
    uint32_t sessionId;
    uint32_t sequence;
};

/**
 * @brief ノード識別情報の固定wire形式を表します。
 */
struct RangingNodeWireIdentity
{
    uint8_t nodeId;
    uint8_t macAddress[6];
    char uwbAddress[8];
};

/**
 * @brief 測距制御の固定wire packetを表します。
 */
struct RangeControlPacket
{
    SequentialRangingPacketHeader header;
    uint32_t roundId;
    uint16_t pairSequence;
    uint8_t masterTagId;
    uint8_t masterMac[6];
    uint8_t anchorCount;
    uint8_t tagCount;
    uint8_t anchorIndex;
    uint8_t tagIndex;
    uint8_t anchorIds[8];
    uint8_t tagIds[8];
};

/**
 * @brief 逐次測距結果と転送結果の固定wire packetを表します。
 */
struct RangeMeasurementPacket
{
    SequentialRangingPacketHeader header;
    uint32_t roundId;
    uint16_t pairSequence;
    uint8_t masterTagId;
    uint8_t masterMac[6];
    uint8_t anchorCount;
    uint8_t tagCount;
    uint8_t anchorIndex;
    uint8_t tagIndex;
    RangingNodeWireIdentity anchor;
    RangingNodeWireIdentity tag;
    uint8_t status;
    uint32_t distanceMm;
    int16_t uwbRssi;
    uint32_t commandReceivedUs;
    uint32_t rangingStartedUs;
    uint32_t rangingCompletedUs;
    int8_t espNowRssi;
    uint64_t commandReceivedMasterTimeUs;
    uint64_t rangingStartedMasterTimeUs;
    uint64_t rangingCompletedMasterTimeUs;
    uint32_t synchronizationRoundTripUs;
    uint64_t synchronizationAgeUs;
    uint8_t timeQuality;
    uint8_t isLastMeasurement;
};

/**
 * @brief ラウンド完了の固定wire packetを表します。
 */
struct RangeRoundCompletePacket
{
    SequentialRangingPacketHeader header;
    uint32_t roundId;
    uint32_t nextRoundId;
    uint8_t masterTagId;
    uint8_t masterMac[6];
    uint64_t startedMasterTimeUs;
    uint64_t completedMasterTimeUs;
    uint8_t anchorCount;
    uint8_t tagCount;
    uint8_t expectedMeasurementCount;
    uint8_t receivedMeasurementCount;
    uint64_t missingMeasurementBits;
    uint8_t anchorListTruncated;
    uint8_t tagListTruncated;
    uint8_t timedOut;
};
#pragma pack(pop)

/**
 * @brief 逐次測距domain値と固定wire形式を相互変換します。
 */
class SequentialRangingProtocolCodec
{
public:
    static constexpr uint16_t m_magic = 0x5259;
    static constexpr uint8_t m_version = 1;
    static constexpr uint8_t m_maxAnchorCount = 8;
    static constexpr uint8_t m_maxTagCount = 8;

    /**
     * @brief payloadが逐次測距packet種別か確認します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @return 逐次測距packet種別の場合はtrue、それ以外はfalse
     */
    static bool IsSequentialRangingPacket(const uint8_t* data, size_t length);

    /**
     * @brief 測距制御を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param data 変換する測距制御
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeControl(uint32_t sessionId, uint32_t sequence,
        const RangeControlData& data, RangeControlPacket& packet);

    /**
     * @brief 固定wire形式の測距制御を検証して読み出します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param sessionId 検証済みマスターセッションID格納先
     * @param sequence 検証済みpacketシーケンス格納先
     * @param control 検証済み測距制御格納先
     * @return packetが有効な場合はtrue、それ以外はfalse
     */
    static bool DecodeControl(const uint8_t* data, size_t length,
        uint32_t& sessionId, uint32_t& sequence, RangeControlData& control);

    /**
     * @brief ANCHORの逐次測距結果を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param data 変換する測距結果
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeMeasurement(uint32_t sessionId, uint32_t sequence,
        const RangeMeasurementData& data, RangeMeasurementPacket& packet);

    /**
     * @brief マスター時刻へ変換した逐次測距結果を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param data 変換する測距結果
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeMeasurementForward(uint32_t sessionId, uint32_t sequence,
        const RangeMeasurementData& data, RangeMeasurementPacket& packet);

    /**
     * @brief 固定wire形式の逐次測距結果を検証して読み出します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param expectedType 期待する測距結果packet種別
     * @param sessionId 検証済みマスターセッションID格納先
     * @param sequence 検証済みpacketシーケンス格納先
     * @param measurement 検証済み測距結果格納先
     * @return packetが有効な場合はtrue、それ以外はfalse
     */
    static bool DecodeMeasurement(const uint8_t* data, size_t length,
        EnSequentialRangingPacketType expectedType, uint32_t& sessionId,
        uint32_t& sequence, RangeMeasurementData& measurement);

    /**
     * @brief ラウンド完了情報を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param data 変換するラウンド完了情報
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeRoundComplete(uint32_t sessionId, uint32_t sequence,
        const RangeRoundCompleteData& data, RangeRoundCompletePacket& packet);

    /**
     * @brief 固定wire形式のラウンド完了情報を検証して読み出します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param sessionId 検証済みマスターセッションID格納先
     * @param sequence 検証済みpacketシーケンス格納先
     * @param complete 検証済みラウンド完了情報格納先
     * @return packetが有効な場合はtrue、それ以外はfalse
     */
    static bool DecodeRoundComplete(const uint8_t* data, size_t length,
        uint32_t& sessionId, uint32_t& sequence, RangeRoundCompleteData& complete);
};

static_assert(sizeof(SequentialRangingPacketHeader) == 12, "ranging header size mismatch");
static_assert(sizeof(RangingNodeWireIdentity) == 15, "ranging identity size mismatch");
static_assert(sizeof(RangeControlPacket) == 45, "range control size mismatch");
static_assert(sizeof(RangeMeasurementPacket) == 117, "range measurement size mismatch");
static_assert(sizeof(RangeRoundCompletePacket) == 58, "range complete size mismatch");
static_assert(sizeof(SequentialRangingPacketHeader) <= 250, "ranging header exceeds ESP-NOW v1 payload");
static_assert(sizeof(RangingNodeWireIdentity) <= 250, "ranging identity exceeds ESP-NOW v1 payload");
static_assert(sizeof(RangeControlPacket) <= 250, "range control exceeds ESP-NOW v1 payload");
static_assert(sizeof(RangeMeasurementPacket) <= 250, "range measurement exceeds ESP-NOW v1 payload");
static_assert(sizeof(RangeRoundCompletePacket) <= 250, "range complete exceeds ESP-NOW v1 payload");
