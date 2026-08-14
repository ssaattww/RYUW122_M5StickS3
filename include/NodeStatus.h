#pragma once

#include <cstddef>
#include <cstdint>

#include "RunMode.h"

/**
 * @brief ノード状態packetの種別を表します。
 */
enum class EnNodeStatusPacketType : uint8_t
{
    NodeStatus = 1,
};

/**
 * @brief アプリケーション内で使用するノード状態を表します。
 */
struct NodeStatus
{
    uint16_t anchorPositionX = 0;
    uint16_t anchorPositionY = 0;
    uint8_t macAddress[6]{};
    uint8_t nodeID = 0;
    EnRunMode mode = EnRunMode::Anchor;
    char uwbAddress[9]{};
    bool isMaster = false;
    uint32_t sessionId = 0;
};

#pragma pack(push, 1)
/**
 * @brief NodeStatus version 2の固定wire形式を表します。
 */
struct NodeStatusWirePacket
{
    uint16_t magic;
    uint8_t version;
    uint8_t packetType;
    uint8_t nodeID;
    uint8_t mode;
    uint8_t macAddress[6];
    char uwbAddress[8];
    uint16_t anchorPositionX;
    uint16_t anchorPositionY;
    uint8_t masterDeclared;
    uint32_t sessionId;
};
#pragma pack(pop)

/**
 * @brief NodeStatusのdomain値と固定wire形式を相互変換します。
 */
class NodeStatusCodec
{
public:
    static constexpr uint16_t m_magic = 0x5259;
    static constexpr uint8_t m_version = 2;
    static constexpr size_t m_wireSize = 29;

    /**
     * @brief payloadがNodeStatus packet種別か確認します。
     * versionや内容の妥当性はDecode()で別途検証します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @return NodeStatus packet種別の場合はtrue、それ以外はfalse
     */
    static bool IsNodeStatusPacket(
        const uint8_t* data,
        size_t length);

    /**
     * @brief ノード状態をversion 2のwire形式へ変換します。
     *
     * @param status 変換するノード状態
     * @param packet 変換後のwire packet格納先
     * @return ノード状態が有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool Encode(
        const NodeStatus& status,
        NodeStatusWirePacket& packet);

    /**
     * @brief version 2のwire形式をノード状態へ変換します。
     *
     * @param data 受信payload
     * @param length 受信payloadサイズ
     * @param sourceMac transportから取得した送信元MACアドレス
     * @param status 変換後のノード状態格納先
     * @return packetが有効で変換できた場合はtrue、それ以外はfalse
     */
    static bool Decode(
        const uint8_t* data,
        size_t length,
        const uint8_t sourceMac[6],
        NodeStatus& status);
};

static_assert(
    sizeof(NodeStatusWirePacket) == NodeStatusCodec::m_wireSize,
    "NodeStatus wire size mismatch");
static_assert(
    sizeof(NodeStatusWirePacket) <= 250,
    "NodeStatus wire packet exceeds ESP-NOW v1 payload");
