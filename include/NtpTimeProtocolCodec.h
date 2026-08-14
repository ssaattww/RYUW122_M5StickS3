#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief NTP時刻同期packetの種別を表します。
 */
enum class EnNtpPacketType : uint8_t
{
    SyncRequest = 2,
    SyncResponse = 3,
    SyncCommit = 4,
};

/**
 * @brief 公開する時刻品質を表します。
 */
enum class EnTimeQuality : uint8_t
{
    Synchronized = 0,
    PowerSaveEnabled = 1,
    ReceiveTimestampUnavailable = 2,
    SynchronizationExpired = 3,
    Unsynchronized = 4,
};

#pragma pack(push, 1)
/**
 * @brief NTP時刻同期packetの固定共通headerを表します。
 */
struct NtpPacketHeader
{
    uint16_t magic;
    uint8_t version;
    uint8_t packetType;
    uint32_t sessionId;
    uint32_t sequence;
};

/**
 * @brief マスターTAGから対象ノードへ送る同期要求を表します。
 */
struct NtpSyncRequestPacket
{
    NtpPacketHeader header;
    uint8_t masterNodeId;
    uint8_t masterMac[6];
    uint8_t targetNodeId;
    uint32_t t1;
};

/**
 * @brief 対象ノードからマスターTAGへ返す同期応答を表します。
 */
struct NtpSyncResponsePacket
{
    NtpPacketHeader header;
    uint8_t targetNodeId;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint8_t receiveTimestampAvailable;
    uint8_t powerSaveEnabled;
};

/**
 * @brief マスターTAGからフォロワーTAGへ送る同期確定通知を表します。
 */
struct NtpSyncCommitPacket
{
    NtpPacketHeader header;
    uint8_t targetNodeId;
    int64_t nodeMinusMasterUs;
    uint32_t roundTripUs;
    uint8_t timeQuality;
    uint64_t synchronizedAtMasterTimeUs;
};
#pragma pack(pop)

/**
 * @brief NTP時刻同期domain値と固定wire形式を相互変換します。
 */
class NtpTimeProtocolCodec
{
public:
    static constexpr uint16_t m_magic = 0x5259;
    static constexpr uint8_t m_version = 1;

    /**
     * @brief payloadがNTP時刻同期packet種別か確認します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @return NTP時刻同期packet種別の場合はtrue、それ以外はfalse
     */
    static bool IsNtpPacket(const uint8_t* data, size_t length);

    /**
     * @brief 同期要求を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence 要求シーケンス
     * @param masterNodeId マスターTAGのノードID
     * @param masterMac マスターTAGのMACアドレス
     * @param targetNodeId 対象ノードID
     * @param t1 マスターTAGの要求送信直前時刻
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeRequest(
        uint32_t sessionId,
        uint32_t sequence,
        uint8_t masterNodeId,
        const uint8_t masterMac[6],
        uint8_t targetNodeId,
        uint32_t t1,
        NtpSyncRequestPacket& packet);

    /**
     * @brief 固定wire形式の同期要求を検証して読み出します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param packet 検証済みwire packet格納先
     * @return packetが有効な場合はtrue、それ以外はfalse
     */
    static bool DecodeRequest(
        const uint8_t* data,
        size_t length,
        NtpSyncRequestPacket& packet);

    /**
     * @brief 同期応答を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence 要求シーケンス
     * @param targetNodeId 対象ノードID
     * @param t1 要求から受け取ったマスターTAG時刻
     * @param t2 対象ノードの要求受信時刻
     * @param t3 対象ノードの応答送信直前時刻
     * @param receiveTimestampAvailable t2にrx_ctrl timestampを使用した場合はtrue
     * @param powerSaveEnabled 対象ノードのWi-Fi省電力が有効な場合はtrue
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeResponse(
        uint32_t sessionId,
        uint32_t sequence,
        uint8_t targetNodeId,
        uint32_t t1,
        uint32_t t2,
        uint32_t t3,
        bool receiveTimestampAvailable,
        bool powerSaveEnabled,
        NtpSyncResponsePacket& packet);

    /**
     * @brief 固定wire形式の同期応答を検証して読み出します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param packet 検証済みwire packet格納先
     * @return packetが有効な場合はtrue、それ以外はfalse
     */
    static bool DecodeResponse(
        const uint8_t* data,
        size_t length,
        NtpSyncResponsePacket& packet);

    /**
     * @brief 同期確定通知を固定wire形式へ変換します。
     *
     * @param sessionId マスターセッションID
     * @param sequence 通知シーケンス
     * @param targetNodeId 対象フォロワーTAG ID
     * @param nodeMinusMasterUs 対象時計からマスター時計へのoffset
     * @param roundTripUs 採用した往復遅延
     * @param timeQuality 採用した時刻品質
     * @param synchronizedAtMasterTimeUs 同期確定時のマスター64bit時刻
     * @param packet 変換後のwire packet格納先
     * @return 入力が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool EncodeCommit(
        uint32_t sessionId,
        uint32_t sequence,
        uint8_t targetNodeId,
        int64_t nodeMinusMasterUs,
        uint32_t roundTripUs,
        EnTimeQuality timeQuality,
        uint64_t synchronizedAtMasterTimeUs,
        NtpSyncCommitPacket& packet);

    /**
     * @brief 固定wire形式の同期確定通知を検証して読み出します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param packet 検証済みwire packet格納先
     * @return packetが有効な場合はtrue、それ以外はfalse
     */
    static bool DecodeCommit(
        const uint8_t* data,
        size_t length,
        NtpSyncCommitPacket& packet);

private:
    /**
     * @brief 共通headerを指定packet種別で初期化します。
     *
     * @param packetType packet種別
     * @param sessionId マスターセッションID
     * @param sequence packetシーケンス
     * @param header 初期化する共通header
     */
    static void InitializeHeader(
        EnNtpPacketType packetType,
        uint32_t sessionId,
        uint32_t sequence,
        NtpPacketHeader& header);

    /**
     * @brief 共通headerと期待packet種別を検証します。
     *
     * @param header 検証する共通header
     * @param packetType 期待packet種別
     * @return 共通headerが有効な場合はtrue、それ以外はfalse
     */
    static bool IsValidHeader(
        const NtpPacketHeader& header,
        EnNtpPacketType packetType);
};

static_assert(sizeof(NtpPacketHeader) == 12, "NTP header size mismatch");
static_assert(sizeof(NtpSyncRequestPacket) == 24, "NTP request size mismatch");
static_assert(sizeof(NtpSyncResponsePacket) == 27, "NTP response size mismatch");
static_assert(sizeof(NtpSyncCommitPacket) == 34, "NTP commit size mismatch");
static_assert(sizeof(NtpSyncRequestPacket) <= 250, "NTP request exceeds ESP-NOW v1 payload");
static_assert(sizeof(NtpSyncResponsePacket) <= 250, "NTP response exceeds ESP-NOW v1 payload");
static_assert(sizeof(NtpSyncCommitPacket) <= 250, "NTP commit exceeds ESP-NOW v1 payload");
