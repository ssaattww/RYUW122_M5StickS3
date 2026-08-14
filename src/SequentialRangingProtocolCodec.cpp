#include "SequentialRangingProtocolCodec.h"

#include <cstring>

namespace
{
    /**
     * @brief MACアドレスがゼロまたはbroadcastでないか確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return 有効なMACアドレスの場合はtrue、それ以外はfalse
     */
    bool IsValidMacAddress(const uint8_t macAddress[6])
    {
        bool allZero = true;
        bool allBroadcast = true;
        for (size_t index = 0; index < 6; ++index)
        {
            allZero = allZero && macAddress[index] == 0U;
            allBroadcast = allBroadcast && macAddress[index] == 0xffU;
        }
        return !allZero && !allBroadcast;
    }

    /**
     * @brief 8文字のUWBアドレスがwireへ変換可能か確認します。
     *
     * @param address 確認するUWBアドレス
     * @return 8文字すべてが印字可能ASCIIの場合はtrue、それ以外はfalse
     */
    bool IsValidUwbAddress(const char address[9])
    {
        if (address[8] != '\0')
        {
            return false;
        }
        for (size_t index = 0; index < 8; ++index)
        {
            const uint8_t character = static_cast<uint8_t>(address[index]);
            if (character < 0x21U || character > 0x7eU)
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief ノード識別情報が有効か確認します。
     *
     * @param identity 確認するノード識別情報
     * @return MACとUWBアドレスが有効な場合はtrue、それ以外はfalse
     */
    bool IsValidIdentity(const RangingNodeIdentity& identity)
    {
        return IsValidMacAddress(identity.macAddress) &&
            IsValidUwbAddress(identity.uwbAddress);
    }

    /**
     * @brief domain識別情報を固定wire形式へ変換します。
     *
     * @param source 変換元識別情報
     * @param destination 変換先wire識別情報
     */
    void EncodeIdentity(const RangingNodeIdentity& source,
        RangingNodeWireIdentity& destination)
    {
        destination.nodeId = source.nodeId;
        memcpy(destination.macAddress, source.macAddress,
            sizeof(destination.macAddress));
        memcpy(destination.uwbAddress, source.uwbAddress,
            sizeof(destination.uwbAddress));
    }

    /**
     * @brief 固定wire識別情報をdomain値へ変換します。
     *
     * @param source 変換元wire識別情報
     * @param destination 変換先識別情報
     */
    void DecodeIdentity(const RangingNodeWireIdentity& source,
        RangingNodeIdentity& destination)
    {
        destination = RangingNodeIdentity{};
        destination.nodeId = source.nodeId;
        memcpy(destination.macAddress, source.macAddress,
            sizeof(destination.macAddress));
        memcpy(destination.uwbAddress, source.uwbAddress,
            sizeof(source.uwbAddress));
        destination.uwbAddress[8] = '\0';
    }

    /**
     * @brief 共通headerを初期化します。
     *
     * @param type packet種別
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param header 初期化する共通header
     */
    void InitializeHeader(EnSequentialRangingPacketType type,
        uint32_t sessionId, uint32_t sequence,
        SequentialRangingPacketHeader& header)
    {
        header.magic = SequentialRangingProtocolCodec::m_magic;
        header.version = SequentialRangingProtocolCodec::m_version;
        header.packetType = static_cast<uint8_t>(type);
        header.sessionId = sessionId;
        header.sequence = sequence;
    }

    /**
     * @brief 共通headerが期待するpacket種別か確認します。
     *
     * @param header 確認する共通header
     * @param type 期待するpacket種別
     * @return headerが有効な場合はtrue、それ以外はfalse
     */
    bool IsValidHeader(const SequentialRangingPacketHeader& header,
        EnSequentialRangingPacketType type)
    {
        return header.magic == SequentialRangingProtocolCodec::m_magic &&
            header.version == SequentialRangingProtocolCodec::m_version &&
            header.packetType == static_cast<uint8_t>(type) &&
            header.sessionId != 0U && header.sequence != 0U;
    }

    /**
     * @brief ID一覧に重複がないか確認します。
     *
     * @param anchorIds ANCHOR ID一覧
     * @param anchorCount ANCHOR数
     * @param tagIds TAG ID一覧
     * @param tagCount TAG数
     * @return 全ノードIDが一意の場合はtrue、それ以外はfalse
     */
    bool HasUniqueNodeIds(const uint8_t anchorIds[8], uint8_t anchorCount,
        const uint8_t tagIds[8], uint8_t tagCount)
    {
        for (uint8_t left = 0; left < anchorCount; ++left)
        {
            for (uint8_t right = static_cast<uint8_t>(left + 1U);
                right < anchorCount; ++right)
            {
                if (anchorIds[left] == anchorIds[right])
                {
                    return false;
                }
            }
            for (uint8_t tagIndex = 0; tagIndex < tagCount; ++tagIndex)
            {
                if (anchorIds[left] == tagIds[tagIndex])
                {
                    return false;
                }
            }
        }
        for (uint8_t left = 0; left < tagCount; ++left)
        {
            for (uint8_t right = static_cast<uint8_t>(left + 1U);
                right < tagCount; ++right)
            {
                if (tagIds[left] == tagIds[right])
                {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * @brief ID一覧が昇順か確認します。
     *
     * @param nodeIds 確認するノードID一覧
     * @param count 確認する件数
     * @return 厳密な昇順の場合はtrue、それ以外はfalse
     */
    bool IsStrictlyAscending(const uint8_t nodeIds[8], uint8_t count)
    {
        for (uint8_t index = 1; index < count; ++index)
        {
            if (nodeIds[index - 1U] >= nodeIds[index])
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 測距制御の値が有効か確認します。
     *
     * @param data 確認する測距制御
     * @return 値が有効な場合はtrue、それ以外はfalse
     */
    bool IsValidControl(const RangeControlData& data)
    {
        if (data.roundId == 0U || data.pairSequence == 0U ||
            !IsValidMacAddress(data.masterMac) || data.anchorCount == 0U ||
            data.anchorCount > SequentialRangingProtocolCodec::m_maxAnchorCount ||
            data.tagCount == 0U ||
            data.tagCount > SequentialRangingProtocolCodec::m_maxTagCount ||
            data.anchorIndex >= data.anchorCount || data.tagIndex >= data.tagCount ||
            !HasUniqueNodeIds(data.anchorIds, data.anchorCount,
                data.tagIds, data.tagCount) ||
            !IsStrictlyAscending(data.anchorIds, data.anchorCount) ||
            !IsStrictlyAscending(data.tagIds, data.tagCount))
        {
            return false;
        }
        const uint16_t expectedPair = static_cast<uint16_t>(
            static_cast<uint16_t>(data.anchorIndex) * data.tagCount +
            data.tagIndex + 1U);
        if (data.pairSequence != expectedPair)
        {
            return false;
        }
        for (uint8_t index = 0; index < data.tagCount; ++index)
        {
            if (data.tagIds[index] == data.masterTagId)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 32bitローカル時刻が折り返しを含めて前後関係を保つか確認します。
     *
     * @param earlier 先行時刻
     * @param later 後続時刻
     * @return 差が32bit範囲の半分未満の場合はtrue、それ以外はfalse
     */
    bool IsForwardLocalTime(uint32_t earlier, uint32_t later)
    {
        return static_cast<uint32_t>(later - earlier) <= 0x7fffffffU;
    }

    /**
     * @brief 時刻品質のwire値が定義済みか確認します。
     *
     * @param quality 確認するwire値
     * @return 定義済みの場合はtrue、それ以外はfalse
     */
    bool IsValidTimeQuality(uint8_t quality)
    {
        return quality <= static_cast<uint8_t>(EnTimeQuality::Unsynchronized);
    }

    /**
     * @brief 測距結果状態のwire値が定義済みか確認します。
     *
     * @param status 確認するwire値
     * @return 定義済みの場合はtrue、それ以外はfalse
     */
    bool IsValidResultStatus(uint8_t status)
    {
        return status <= static_cast<uint8_t>(EnRangeResultStatus::Unreachable);
    }

    /**
     * @brief 測距結果の共通値が有効か確認します。
     *
     * @param data 確認する測距結果
     * @return 値が有効な場合はtrue、それ以外はfalse
     */
    bool IsValidMeasurementCommon(const RangeMeasurementData& data)
    {
        if (data.roundId == 0U || data.pairSequence == 0U ||
            data.pairSequence > 64U || !IsValidMacAddress(data.masterMac) ||
            data.anchorCount == 0U ||
            data.anchorCount > SequentialRangingProtocolCodec::m_maxAnchorCount ||
            data.tagCount == 0U ||
            data.tagCount > SequentialRangingProtocolCodec::m_maxTagCount ||
            data.anchorIndex >= data.anchorCount || data.tagIndex >= data.tagCount ||
            data.anchor.nodeId == data.tag.nodeId ||
            !IsValidIdentity(data.anchor) || !IsValidIdentity(data.tag) ||
            !IsValidResultStatus(static_cast<uint8_t>(data.status)) ||
            data.uwbRssi > 0 ||
            data.uwbRssi < -200 ||
            !IsForwardLocalTime(data.commandReceivedUs, data.rangingStartedUs) ||
            !IsForwardLocalTime(data.rangingStartedUs, data.rangingCompletedUs))
        {
            return false;
        }
        const uint16_t expectedPair = static_cast<uint16_t>(
            static_cast<uint16_t>(data.anchorIndex) * data.tagCount +
            data.tagIndex + 1U);
        const bool expectedLast =
            data.anchorIndex == static_cast<uint8_t>(data.anchorCount - 1U) &&
            data.tagIndex == static_cast<uint8_t>(data.tagCount - 1U);
        if (data.pairSequence != expectedPair ||
            data.isLastMeasurement != expectedLast ||
            memcmp(data.anchor.macAddress, data.tag.macAddress,
                sizeof(data.anchor.macAddress)) == 0 ||
            memcmp(data.anchor.uwbAddress, data.tag.uwbAddress, 8) == 0)
        {
            return false;
        }
        if (data.status != EnRangeResultStatus::Success &&
            (data.distanceMm != 0U || data.uwbRssi != 0))
        {
            return false;
        }
        if (data.tag.nodeId == data.masterTagId &&
            memcmp(data.tag.macAddress, data.masterMac,
                sizeof(data.masterMac)) != 0)
        {
            return false;
        }
        return true;
    }

    /**
     * @brief 測距結果の時刻domainがpacket種別と整合するか確認します。
     *
     * @param data 確認する測距結果
     * @param isForward フォロワー転送の場合はtrue
     * @return 時刻domainが整合する場合はtrue、それ以外はfalse
     */
    bool IsValidMeasurementTimeDomain(const RangeMeasurementData& data,
        bool isForward)
    {
        if (!IsValidTimeQuality(static_cast<uint8_t>(data.timeQuality)))
        {
            return false;
        }
        if (!isForward)
        {
            return data.commandReceivedMasterTimeUs == 0U &&
                data.rangingStartedMasterTimeUs == 0U &&
                data.rangingCompletedMasterTimeUs == 0U &&
                data.synchronizationRoundTripUs == 0U &&
                data.synchronizationAgeUs == 0U &&
                data.timeQuality == EnTimeQuality::Unsynchronized;
        }
        return data.timeQuality != EnTimeQuality::Unsynchronized &&
            data.commandReceivedMasterTimeUs <=
                data.rangingStartedMasterTimeUs &&
            data.rangingStartedMasterTimeUs <=
                data.rangingCompletedMasterTimeUs;
    }

    /**
     * @brief 測距結果を指定packet種別で固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param type packet種別
     * @param data 変換する測距結果
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    bool EncodeMeasurementForType(uint32_t sessionId, uint32_t sequence,
        EnSequentialRangingPacketType type, const RangeMeasurementData& data,
        RangeMeasurementPacket& packet)
    {
        const bool isForward =
            type == EnSequentialRangingPacketType::RangeMeasurementForward;
        if (sessionId == 0U || sequence == 0U ||
            !IsValidMeasurementCommon(data) ||
            !IsValidMeasurementTimeDomain(data, isForward))
        {
            return false;
        }

        RangeMeasurementPacket encoded{};
        InitializeHeader(type, sessionId, sequence, encoded.header);
        encoded.roundId = data.roundId;
        encoded.pairSequence = data.pairSequence;
        encoded.masterTagId = data.masterTagId;
        memcpy(encoded.masterMac, data.masterMac, sizeof(encoded.masterMac));
        encoded.anchorCount = data.anchorCount;
        encoded.tagCount = data.tagCount;
        encoded.anchorIndex = data.anchorIndex;
        encoded.tagIndex = data.tagIndex;
        EncodeIdentity(data.anchor, encoded.anchor);
        EncodeIdentity(data.tag, encoded.tag);
        encoded.status = static_cast<uint8_t>(data.status);
        encoded.distanceMm = data.distanceMm;
        encoded.uwbRssi = data.uwbRssi;
        encoded.commandReceivedUs = data.commandReceivedUs;
        encoded.rangingStartedUs = data.rangingStartedUs;
        encoded.rangingCompletedUs = data.rangingCompletedUs;
        encoded.espNowRssi = data.espNowRssi;
        encoded.commandReceivedMasterTimeUs = data.commandReceivedMasterTimeUs;
        encoded.rangingStartedMasterTimeUs = data.rangingStartedMasterTimeUs;
        encoded.rangingCompletedMasterTimeUs = data.rangingCompletedMasterTimeUs;
        encoded.synchronizationRoundTripUs = data.synchronizationRoundTripUs;
        encoded.synchronizationAgeUs = data.synchronizationAgeUs;
        encoded.timeQuality = static_cast<uint8_t>(data.timeQuality);
        encoded.isLastMeasurement = data.isLastMeasurement ? 1U : 0U;
        packet = encoded;
        return true;
    }

    /**
     * @brief 64bit値に含まれる1のbit数を数えます。
     *
     * @param value 確認する値
     * @return 1に設定されたbit数
     */
    uint8_t CountBits(uint64_t value)
    {
        uint8_t count = 0;
        while (value != 0U)
        {
            value &= value - 1U;
            ++count;
        }
        return count;
    }

    /**
     * @brief ラウンド完了情報が有効か確認します。
     *
     * @param data 確認するラウンド完了情報
     * @return 値が有効な場合はtrue、それ以外はfalse
     */
    bool IsValidRoundComplete(const RangeRoundCompleteData& data)
    {
        if (data.roundId == 0U || data.nextRoundId == 0U ||
            data.nextRoundId == data.roundId ||
            !IsValidMacAddress(data.masterMac) || data.anchorCount == 0U ||
            data.anchorCount > SequentialRangingProtocolCodec::m_maxAnchorCount ||
            data.tagCount == 0U ||
            data.tagCount > SequentialRangingProtocolCodec::m_maxTagCount ||
            data.startedMasterTimeUs > data.completedMasterTimeUs)
        {
            return false;
        }
        const uint8_t expected = static_cast<uint8_t>(
            data.anchorCount * data.tagCount);
        if (data.expectedMeasurementCount != expected ||
            data.receivedMeasurementCount > expected)
        {
            return false;
        }
        const uint64_t validMask = expected == 64U
            ? UINT64_MAX
            : (UINT64_C(1) << expected) - 1U;
        if ((data.missingMeasurementBits & ~validMask) != 0U ||
            CountBits(data.missingMeasurementBits) !=
                static_cast<uint8_t>(expected - data.receivedMeasurementCount))
        {
            return false;
        }
        return data.timedOut == (data.missingMeasurementBits != 0U);
    }
}

bool SequentialRangingProtocolCodec::IsSequentialRangingPacket(
    const uint8_t* data, size_t length)
{
    if (data == nullptr || length < sizeof(SequentialRangingPacketHeader))
    {
        return false;
    }
    SequentialRangingPacketHeader header{};
    memcpy(&header, data, sizeof(header));
    return header.magic == m_magic &&
        header.packetType >= static_cast<uint8_t>(
            EnSequentialRangingPacketType::RangeControl) &&
        header.packetType <= static_cast<uint8_t>(
            EnSequentialRangingPacketType::RangeRoundComplete);
}

bool SequentialRangingProtocolCodec::EncodeControl(uint32_t sessionId,
    uint32_t sequence, const RangeControlData& data, RangeControlPacket& packet)
{
    if (sessionId == 0U || sequence == 0U || !IsValidControl(data))
    {
        return false;
    }
    RangeControlPacket encoded{};
    InitializeHeader(EnSequentialRangingPacketType::RangeControl,
        sessionId, sequence, encoded.header);
    encoded.roundId = data.roundId;
    encoded.pairSequence = data.pairSequence;
    encoded.masterTagId = data.masterTagId;
    memcpy(encoded.masterMac, data.masterMac, sizeof(encoded.masterMac));
    encoded.anchorCount = data.anchorCount;
    encoded.tagCount = data.tagCount;
    encoded.anchorIndex = data.anchorIndex;
    encoded.tagIndex = data.tagIndex;
    memcpy(encoded.anchorIds, data.anchorIds, sizeof(encoded.anchorIds));
    memcpy(encoded.tagIds, data.tagIds, sizeof(encoded.tagIds));
    packet = encoded;
    return true;
}

bool SequentialRangingProtocolCodec::DecodeControl(const uint8_t* data,
    size_t length, uint32_t& sessionId, uint32_t& sequence,
    RangeControlData& control)
{
    if (data == nullptr || length != sizeof(RangeControlPacket))
    {
        return false;
    }
    RangeControlPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!IsValidHeader(packet.header,
        EnSequentialRangingPacketType::RangeControl))
    {
        return false;
    }
    RangeControlData decoded{};
    decoded.roundId = packet.roundId;
    decoded.pairSequence = packet.pairSequence;
    decoded.masterTagId = packet.masterTagId;
    memcpy(decoded.masterMac, packet.masterMac, sizeof(decoded.masterMac));
    decoded.anchorCount = packet.anchorCount;
    decoded.tagCount = packet.tagCount;
    decoded.anchorIndex = packet.anchorIndex;
    decoded.tagIndex = packet.tagIndex;
    memcpy(decoded.anchorIds, packet.anchorIds, sizeof(decoded.anchorIds));
    memcpy(decoded.tagIds, packet.tagIds, sizeof(decoded.tagIds));
    if (!IsValidControl(decoded))
    {
        return false;
    }
    sessionId = packet.header.sessionId;
    sequence = packet.header.sequence;
    control = decoded;
    return true;
}

bool SequentialRangingProtocolCodec::EncodeMeasurement(uint32_t sessionId,
    uint32_t sequence, const RangeMeasurementData& data,
    RangeMeasurementPacket& packet)
{
    return EncodeMeasurementForType(sessionId, sequence,
        EnSequentialRangingPacketType::RangeMeasurement, data, packet);
}

bool SequentialRangingProtocolCodec::EncodeMeasurementForward(
    uint32_t sessionId, uint32_t sequence, const RangeMeasurementData& data,
    RangeMeasurementPacket& packet)
{
    return EncodeMeasurementForType(sessionId, sequence,
        EnSequentialRangingPacketType::RangeMeasurementForward, data, packet);
}

bool SequentialRangingProtocolCodec::DecodeMeasurement(const uint8_t* data,
    size_t length, EnSequentialRangingPacketType expectedType,
    uint32_t& sessionId, uint32_t& sequence,
    RangeMeasurementData& measurement)
{
    if (data == nullptr || length != sizeof(RangeMeasurementPacket) ||
        (expectedType != EnSequentialRangingPacketType::RangeMeasurement &&
         expectedType != EnSequentialRangingPacketType::RangeMeasurementForward))
    {
        return false;
    }
    RangeMeasurementPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!IsValidHeader(packet.header, expectedType) ||
        packet.isLastMeasurement > 1U)
    {
        return false;
    }
    RangeMeasurementData decoded{};
    decoded.roundId = packet.roundId;
    decoded.pairSequence = packet.pairSequence;
    decoded.masterTagId = packet.masterTagId;
    memcpy(decoded.masterMac, packet.masterMac, sizeof(decoded.masterMac));
    decoded.anchorCount = packet.anchorCount;
    decoded.tagCount = packet.tagCount;
    decoded.anchorIndex = packet.anchorIndex;
    decoded.tagIndex = packet.tagIndex;
    DecodeIdentity(packet.anchor, decoded.anchor);
    DecodeIdentity(packet.tag, decoded.tag);
    decoded.status = static_cast<EnRangeResultStatus>(packet.status);
    decoded.distanceMm = packet.distanceMm;
    decoded.uwbRssi = packet.uwbRssi;
    decoded.commandReceivedUs = packet.commandReceivedUs;
    decoded.rangingStartedUs = packet.rangingStartedUs;
    decoded.rangingCompletedUs = packet.rangingCompletedUs;
    decoded.espNowRssi = packet.espNowRssi;
    decoded.commandReceivedMasterTimeUs = packet.commandReceivedMasterTimeUs;
    decoded.rangingStartedMasterTimeUs = packet.rangingStartedMasterTimeUs;
    decoded.rangingCompletedMasterTimeUs = packet.rangingCompletedMasterTimeUs;
    decoded.synchronizationRoundTripUs = packet.synchronizationRoundTripUs;
    decoded.synchronizationAgeUs = packet.synchronizationAgeUs;
    decoded.timeQuality = static_cast<EnTimeQuality>(packet.timeQuality);
    decoded.isLastMeasurement = packet.isLastMeasurement != 0U;
    const bool isForward = expectedType ==
        EnSequentialRangingPacketType::RangeMeasurementForward;
    if (!IsValidMeasurementCommon(decoded) ||
        !IsValidMeasurementTimeDomain(decoded, isForward))
    {
        return false;
    }
    sessionId = packet.header.sessionId;
    sequence = packet.header.sequence;
    measurement = decoded;
    return true;
}

bool SequentialRangingProtocolCodec::EncodeRoundComplete(uint32_t sessionId,
    uint32_t sequence, const RangeRoundCompleteData& data,
    RangeRoundCompletePacket& packet)
{
    if (sessionId == 0U || sequence == 0U || !IsValidRoundComplete(data))
    {
        return false;
    }
    RangeRoundCompletePacket encoded{};
    InitializeHeader(EnSequentialRangingPacketType::RangeRoundComplete,
        sessionId, sequence, encoded.header);
    encoded.roundId = data.roundId;
    encoded.nextRoundId = data.nextRoundId;
    encoded.masterTagId = data.masterTagId;
    memcpy(encoded.masterMac, data.masterMac, sizeof(encoded.masterMac));
    encoded.startedMasterTimeUs = data.startedMasterTimeUs;
    encoded.completedMasterTimeUs = data.completedMasterTimeUs;
    encoded.anchorCount = data.anchorCount;
    encoded.tagCount = data.tagCount;
    encoded.expectedMeasurementCount = data.expectedMeasurementCount;
    encoded.receivedMeasurementCount = data.receivedMeasurementCount;
    encoded.missingMeasurementBits = data.missingMeasurementBits;
    encoded.anchorListTruncated = data.anchorListTruncated ? 1U : 0U;
    encoded.tagListTruncated = data.tagListTruncated ? 1U : 0U;
    encoded.timedOut = data.timedOut ? 1U : 0U;
    packet = encoded;
    return true;
}

bool SequentialRangingProtocolCodec::DecodeRoundComplete(const uint8_t* data,
    size_t length, uint32_t& sessionId, uint32_t& sequence,
    RangeRoundCompleteData& complete)
{
    if (data == nullptr || length != sizeof(RangeRoundCompletePacket))
    {
        return false;
    }
    RangeRoundCompletePacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!IsValidHeader(packet.header,
            EnSequentialRangingPacketType::RangeRoundComplete) ||
        packet.anchorListTruncated > 1U || packet.tagListTruncated > 1U ||
        packet.timedOut > 1U)
    {
        return false;
    }
    RangeRoundCompleteData decoded{};
    decoded.roundId = packet.roundId;
    decoded.nextRoundId = packet.nextRoundId;
    decoded.masterTagId = packet.masterTagId;
    memcpy(decoded.masterMac, packet.masterMac, sizeof(decoded.masterMac));
    decoded.startedMasterTimeUs = packet.startedMasterTimeUs;
    decoded.completedMasterTimeUs = packet.completedMasterTimeUs;
    decoded.anchorCount = packet.anchorCount;
    decoded.tagCount = packet.tagCount;
    decoded.expectedMeasurementCount = packet.expectedMeasurementCount;
    decoded.receivedMeasurementCount = packet.receivedMeasurementCount;
    decoded.missingMeasurementBits = packet.missingMeasurementBits;
    decoded.anchorListTruncated = packet.anchorListTruncated != 0U;
    decoded.tagListTruncated = packet.tagListTruncated != 0U;
    decoded.timedOut = packet.timedOut != 0U;
    if (!IsValidRoundComplete(decoded))
    {
        return false;
    }
    sessionId = packet.header.sessionId;
    sequence = packet.header.sequence;
    complete = decoded;
    return true;
}
