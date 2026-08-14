#include "NodeStatus.h"

#include <cstring>

namespace
{
    /**
     * @brief MACアドレスがゼロまたはbroadcastでないか確認します。
     *
     * @param macAddress 確認するMACアドレス
     * @return 有効なユニキャストMACアドレスの場合はtrue、それ以外はfalse
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
     * @brief 動作モードがwireへ変換可能か確認します。
     *
     * @param mode 確認する動作モード
     * @return TAGまたはANCHORの場合はtrue、それ以外はfalse
     */
    bool IsValidRunMode(EnRunMode mode)
    {
        return mode == EnRunMode::Tag || mode == EnRunMode::Anchor;
    }

    /**
     * @brief 8文字のUWBアドレスが有効か確認します。
     *
     * @param address 確認するUWBアドレス
     * @return 8文字すべてが印字可能ASCIIの場合はtrue、それ以外はfalse
     */
    bool IsValidUwbAddress(const char address[9])
    {
        if (address == nullptr || address[8] != '\0')
        {
            return false;
        }
        for (size_t index = 0; index < 8; ++index)
        {
            const uint8_t character = static_cast<uint8_t>(address[index]);
            if (character < 0x21 || character > 0x7e)
            {
                return false;
            }
        }
        return true;
    }
}

bool NodeStatusCodec::IsNodeStatusPacket(
    const uint8_t* data,
    size_t length)
{
    constexpr size_t PacketTypeOffset = 3;
    if (data == nullptr || length <= PacketTypeOffset)
    {
        return false;
    }

    uint16_t magic = 0;
    memcpy(&magic, data, sizeof(magic));
    return magic == m_magic &&
        data[PacketTypeOffset] == static_cast<uint8_t>(
            EnNodeStatusPacketType::NodeStatus);
}

bool NodeStatusCodec::Encode(
    const NodeStatus& status,
    NodeStatusWirePacket& packet)
{
    if (!IsValidMacAddress(status.macAddress) ||
        !IsValidRunMode(status.mode) ||
        !IsValidUwbAddress(status.uwbAddress) ||
        (status.isMaster &&
         (status.mode != EnRunMode::Tag || status.sessionId == 0)))
    {
        return false;
    }

    packet = NodeStatusWirePacket{};
    packet.magic = m_magic;
    packet.version = m_version;
    packet.packetType = static_cast<uint8_t>(
        EnNodeStatusPacketType::NodeStatus);
    packet.nodeID = status.nodeID;
    packet.mode = static_cast<uint8_t>(status.mode);
    memcpy(packet.macAddress, status.macAddress, sizeof(packet.macAddress));
    memcpy(packet.uwbAddress, status.uwbAddress, sizeof(packet.uwbAddress));
    packet.anchorPositionX = status.anchorPositionX;
    packet.anchorPositionY = status.anchorPositionY;
    packet.masterDeclared = status.isMaster ? 1U : 0U;
    packet.sessionId = status.isMaster ? status.sessionId : 0U;
    return true;
}

bool NodeStatusCodec::Decode(
    const uint8_t* data,
    size_t length,
    const uint8_t sourceMac[6],
    NodeStatus& status)
{
    if (data == nullptr ||
        sourceMac == nullptr ||
        !IsValidMacAddress(sourceMac) ||
        length != sizeof(NodeStatusWirePacket))
    {
        return false;
    }

    NodeStatusWirePacket packet{};
    memcpy(&packet, data, sizeof(packet));
    const EnRunMode mode = static_cast<EnRunMode>(packet.mode);
    if (packet.magic != m_magic ||
        packet.version != m_version ||
        packet.packetType != static_cast<uint8_t>(
            EnNodeStatusPacketType::NodeStatus) ||
        !IsValidRunMode(mode) ||
        memcmp(packet.macAddress, sourceMac, sizeof(packet.macAddress)) != 0 ||
        packet.masterDeclared > 1U ||
        (packet.masterDeclared != 0U &&
         (mode != EnRunMode::Tag || packet.sessionId == 0U)))
    {
        return false;
    }

    status = NodeStatus{};
    status.anchorPositionX = packet.anchorPositionX;
    status.anchorPositionY = packet.anchorPositionY;
    memcpy(status.macAddress, sourceMac, sizeof(status.macAddress));
    status.nodeID = packet.nodeID;
    status.mode = mode;
    memcpy(status.uwbAddress, packet.uwbAddress, sizeof(packet.uwbAddress));
    status.uwbAddress[8] = '\0';
    if (!IsValidUwbAddress(status.uwbAddress))
    {
        return false;
    }
    status.isMaster = packet.masterDeclared != 0U;
    status.sessionId = status.isMaster ? packet.sessionId : 0U;
    return true;
}
