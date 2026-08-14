#include "NtpTimeProtocolCodec.h"

#include <cstring>

namespace
{
    /**
     * @brief MACアドレスが有効なユニキャストアドレスか確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return ゼロまたはbroadcastでない場合はtrue、それ以外はfalse
     */
    bool IsValidMac(const uint8_t macAddress[6])
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
     * @brief 時刻品質のwire値が定義済みか確認します。
     *
     * @param quality 確認するwire値
     * @return 定義済みの場合はtrue、それ以外はfalse
     */
    bool IsValidTimeQuality(uint8_t quality)
    {
        return quality <= static_cast<uint8_t>(EnTimeQuality::Unsynchronized);
    }
}

bool NtpTimeProtocolCodec::IsNtpPacket(
    const uint8_t* data,
    size_t length)
{
    if (data == nullptr || length < sizeof(NtpPacketHeader))
    {
        return false;
    }

    NtpPacketHeader header{};
    memcpy(&header, data, sizeof(header));
    const uint8_t firstType = static_cast<uint8_t>(EnNtpPacketType::SyncRequest);
    const uint8_t lastType = static_cast<uint8_t>(EnNtpPacketType::SyncCommit);
    return header.magic == m_magic &&
        header.packetType >= firstType &&
        header.packetType <= lastType;
}

bool NtpTimeProtocolCodec::EncodeRequest(
    uint32_t sessionId,
    uint32_t sequence,
    uint8_t masterNodeId,
    const uint8_t masterMac[6],
    uint8_t targetNodeId,
    uint32_t t1,
    NtpSyncRequestPacket& packet)
{
    if (sessionId == 0 || sequence == 0 || !IsValidMac(masterMac))
    {
        return false;
    }

    packet = NtpSyncRequestPacket{};
    InitializeHeader(
        EnNtpPacketType::SyncRequest,
        sessionId,
        sequence,
        packet.header);
    packet.masterNodeId = masterNodeId;
    memcpy(packet.masterMac, masterMac, sizeof(packet.masterMac));
    packet.targetNodeId = targetNodeId;
    packet.t1 = t1;
    return true;
}

bool NtpTimeProtocolCodec::DecodeRequest(
    const uint8_t* data,
    size_t length,
    NtpSyncRequestPacket& packet)
{
    if (data == nullptr || length != sizeof(NtpSyncRequestPacket))
    {
        return false;
    }

    NtpSyncRequestPacket decoded{};
    memcpy(&decoded, data, sizeof(decoded));
    if (!IsValidHeader(decoded.header, EnNtpPacketType::SyncRequest) ||
        !IsValidMac(decoded.masterMac))
    {
        return false;
    }
    packet = decoded;
    return true;
}

bool NtpTimeProtocolCodec::EncodeResponse(
    uint32_t sessionId,
    uint32_t sequence,
    uint8_t targetNodeId,
    uint32_t t1,
    uint32_t t2,
    uint32_t t3,
    bool receiveTimestampAvailable,
    bool powerSaveEnabled,
    NtpSyncResponsePacket& packet)
{
    if (sessionId == 0 || sequence == 0)
    {
        return false;
    }

    packet = NtpSyncResponsePacket{};
    InitializeHeader(
        EnNtpPacketType::SyncResponse,
        sessionId,
        sequence,
        packet.header);
    packet.targetNodeId = targetNodeId;
    packet.t1 = t1;
    packet.t2 = t2;
    packet.t3 = t3;
    packet.receiveTimestampAvailable = receiveTimestampAvailable ? 1U : 0U;
    packet.powerSaveEnabled = powerSaveEnabled ? 1U : 0U;
    return true;
}

bool NtpTimeProtocolCodec::DecodeResponse(
    const uint8_t* data,
    size_t length,
    NtpSyncResponsePacket& packet)
{
    if (data == nullptr || length != sizeof(NtpSyncResponsePacket))
    {
        return false;
    }

    NtpSyncResponsePacket decoded{};
    memcpy(&decoded, data, sizeof(decoded));
    if (!IsValidHeader(decoded.header, EnNtpPacketType::SyncResponse) ||
        decoded.receiveTimestampAvailable > 1U ||
        decoded.powerSaveEnabled > 1U)
    {
        return false;
    }
    packet = decoded;
    return true;
}

bool NtpTimeProtocolCodec::EncodeCommit(
    uint32_t sessionId,
    uint32_t sequence,
    uint8_t targetNodeId,
    int64_t nodeMinusMasterUs,
    uint32_t roundTripUs,
    EnTimeQuality timeQuality,
    uint64_t synchronizedAtMasterTimeUs,
    NtpSyncCommitPacket& packet)
{
    if (sessionId == 0 ||
        sequence == 0 ||
        !IsValidTimeQuality(static_cast<uint8_t>(timeQuality)) ||
        timeQuality == EnTimeQuality::Unsynchronized ||
        timeQuality == EnTimeQuality::SynchronizationExpired)
    {
        return false;
    }

    packet = NtpSyncCommitPacket{};
    InitializeHeader(
        EnNtpPacketType::SyncCommit,
        sessionId,
        sequence,
        packet.header);
    packet.targetNodeId = targetNodeId;
    packet.nodeMinusMasterUs = nodeMinusMasterUs;
    packet.roundTripUs = roundTripUs;
    packet.timeQuality = static_cast<uint8_t>(timeQuality);
    packet.synchronizedAtMasterTimeUs = synchronizedAtMasterTimeUs;
    return true;
}

bool NtpTimeProtocolCodec::DecodeCommit(
    const uint8_t* data,
    size_t length,
    NtpSyncCommitPacket& packet)
{
    if (data == nullptr || length != sizeof(NtpSyncCommitPacket))
    {
        return false;
    }

    NtpSyncCommitPacket decoded{};
    memcpy(&decoded, data, sizeof(decoded));
    const EnTimeQuality quality = static_cast<EnTimeQuality>(
        decoded.timeQuality);
    if (!IsValidHeader(decoded.header, EnNtpPacketType::SyncCommit) ||
        !IsValidTimeQuality(decoded.timeQuality) ||
        quality == EnTimeQuality::Unsynchronized ||
        quality == EnTimeQuality::SynchronizationExpired)
    {
        return false;
    }
    packet = decoded;
    return true;
}

void NtpTimeProtocolCodec::InitializeHeader(
    EnNtpPacketType packetType,
    uint32_t sessionId,
    uint32_t sequence,
    NtpPacketHeader& header)
{
    header.magic = m_magic;
    header.version = m_version;
    header.packetType = static_cast<uint8_t>(packetType);
    header.sessionId = sessionId;
    header.sequence = sequence;
}

bool NtpTimeProtocolCodec::IsValidHeader(
    const NtpPacketHeader& header,
    EnNtpPacketType packetType)
{
    return header.magic == m_magic &&
        header.version == m_version &&
        header.packetType == static_cast<uint8_t>(packetType) &&
        header.sessionId != 0 &&
        header.sequence != 0;
}
