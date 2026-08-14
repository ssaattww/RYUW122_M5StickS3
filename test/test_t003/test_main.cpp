#include <unity.h>

#include "EspNowBroadcast.h"
#include "NodeStatus.h"
#include "TagMasterCoordinator.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    uint32_t nextRandom = 0;

    /**
     * @brief テスト用ノード状態を生成します。
     *
     * @param nodeID ノードID
     * @param mode 動作モード
     * @param macTail MACアドレスの末尾値
     * @param isMaster マスター宣言する場合はtrue
     * @param sessionId マスターセッションID
     * @return 生成したノード状態
     */
    NodeStatus MakeStatus(
        uint8_t nodeID,
        EnRunMode mode,
        uint8_t macTail,
        bool isMaster = false,
        uint32_t sessionId = 0)
    {
        NodeStatus status{};
        status.nodeID = nodeID;
        status.mode = mode;
        status.macAddress[0] = 0x02;
        status.macAddress[5] = macTail;
        memcpy(status.uwbAddress, "T0000000", 9);
        status.isMaster = isMaster;
        status.sessionId = sessionId;
        return status;
    }

    /**
     * @brief 指定した相対pathを読めるプロジェクトルートprefixを探索します。
     *
     * @param relativePath プロジェクトルートからの相対path
     * @return 読み出し可能なプロジェクトルートprefix
     */
    std::string FindProjectPrefix(const std::string& relativePath)
    {
        std::string prefix;
        for (size_t depth = 0; depth < 8; ++depth)
        {
            std::ifstream input(prefix + relativePath);
            if (input.good())
            {
                return prefix;
            }
            prefix += "../";
        }
        return std::string{};
    }

    /**
     * @brief テキストファイル全体を読み出します。
     *
     * @param path 読み出すファイルpath
     * @return 読み出したテキスト。失敗時は空文字列
     */
    std::string ReadTextFile(const std::string& path)
    {
        std::ifstream input(path);
        if (!input.good())
        {
            return std::string{};
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

    /**
     * @brief 2つの関数marker間を厳密な関数blockとして抽出します。
     *
     * @param source 抽出元source
     * @param startMarker 対象関数の開始marker
     * @param endMarker 次関数の開始marker
     * @param block 抽出した関数blockの格納先
     * @return markerが順番どおり1回ずつ見つかった場合はtrue
     */
    bool ExtractFunctionBlock(
        const std::string& source,
        const std::string& startMarker,
        const std::string& endMarker,
        std::string& block)
    {
        const size_t start = source.find(startMarker);
        if (start == std::string::npos ||
            source.find(startMarker, start + startMarker.size()) !=
                std::string::npos)
        {
            return false;
        }
        const size_t end = source.find(endMarker, start + startMarker.size());
        if (end == std::string::npos ||
            source.find(endMarker, end + endMarker.size()) !=
                std::string::npos)
        {
            return false;
        }
        block = source.substr(start, end - start);
        return true;
    }

    /**
     * @brief transportとbroadcastの受信所有境界が非破壊契約を満たすか確認します。
     *
     * @param transportHeader EspNowTransport header source
     * @param transportSource EspNowTransport implementation source
     * @param broadcastSource EspNowBroadcast implementation source
     * @return 非NodeStatus packetを破棄しない契約の場合はtrue
     */
    bool HasValidReceiveBoundary(
        const std::string& transportHeader,
        const std::string& transportSource,
        const std::string& broadcastSource)
    {
        if (transportHeader.find("bool PeekReceive(") == std::string::npos ||
            transportHeader.find("bool ConsumeReceive()") == std::string::npos)
        {
            return false;
        }

        std::string peekBlock;
        std::string consumeBlock;
        std::string broadcastUpdateBlock;
        if (!ExtractFunctionBlock(
                transportSource,
                "bool EspNowTransport::PeekReceive(",
                "bool EspNowTransport::ConsumeReceive()",
                peekBlock) ||
            !ExtractFunctionBlock(
                transportSource,
                "bool EspNowTransport::ConsumeReceive()",
                "bool EspNowTransport::TryReceive(",
                consumeBlock) ||
            !ExtractFunctionBlock(
                broadcastSource,
                "void EspNowBroadcast::Update()",
                "bool EspNowBroadcast::TryReceive(",
                broadcastUpdateBlock))
        {
            return false;
        }

        const bool peekDoesNotConsume =
            peekBlock.find("xQueuePeek(") != std::string::npos &&
            peekBlock.find("xQueueReceive(") == std::string::npos;
        const bool consumeRemovesHead =
            consumeBlock.find("xQueueReceive(") != std::string::npos;
        const size_t peekCall = broadcastUpdateBlock.find(
            "m_transport.PeekReceive(");
        const size_t typeCheck = broadcastUpdateBlock.find(
            "NodeStatusCodec::IsNodeStatusPacket(",
            peekCall);
        const size_t consumeCall = broadcastUpdateBlock.find(
            "m_transport.ConsumeReceive(",
            typeCheck);
        const bool broadcastConsumesOwnedPacketOnly =
            peekCall != std::string::npos &&
            typeCheck != std::string::npos &&
            consumeCall != std::string::npos &&
            broadcastUpdateBlock.find("m_transport.TryReceive(") ==
                std::string::npos;
        return peekDoesNotConsume &&
            consumeRemovesHead &&
            broadcastConsumesOwnedPacketOnly;
    }

    /**
     * @brief NodeStatus version 2のcodec往復と送信元MAC整合を検証します。
     */
    void TestNodeStatusCodec()
    {
        NodeStatus source = MakeStatus(7, EnRunMode::Tag, 7, true, 1234);
        source.anchorPositionX = 100;
        source.anchorPositionY = 200;
        NodeStatusWirePacket packet{};
        TEST_ASSERT_TRUE(NodeStatusCodec::Encode(source, packet));
        TEST_ASSERT_EQUAL_UINT32(NodeStatusCodec::m_wireSize, sizeof(packet));
        TEST_ASSERT_TRUE(NodeStatusCodec::IsNodeStatusPacket(
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet)));

        NodeStatusWirePacket otherPacketType = packet;
        ++otherPacketType.packetType;
        TEST_ASSERT_FALSE(NodeStatusCodec::IsNodeStatusPacket(
            reinterpret_cast<const uint8_t*>(&otherPacketType),
            sizeof(otherPacketType)));

        const uint8_t spoofedMac[6] = {0x02, 1, 2, 3, 4, 9};
        NodeStatus decoded{};
        TEST_ASSERT_FALSE(NodeStatusCodec::Decode(
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet),
            spoofedMac,
            decoded));
        TEST_ASSERT_TRUE(NodeStatusCodec::Decode(
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet),
            source.macAddress,
            decoded));
        TEST_ASSERT_EQUAL_UINT8(7, decoded.nodeID);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnRunMode::Tag),
            static_cast<uint8_t>(decoded.mode));
        TEST_ASSERT_EQUAL_UINT16(100, decoded.anchorPositionX);
        TEST_ASSERT_EQUAL_UINT16(200, decoded.anchorPositionY);
        TEST_ASSERT_TRUE(decoded.isMaster);
        TEST_ASSERT_EQUAL_UINT32(1234, decoded.sessionId);
        TEST_ASSERT_EQUAL_MEMORY(
            source.macAddress,
            decoded.macAddress,
            sizeof(source.macAddress));

        NodeStatus invalidAnchorMaster = source;
        invalidAnchorMaster.mode = EnRunMode::Anchor;
        TEST_ASSERT_FALSE(NodeStatusCodec::Encode(invalidAnchorMaster, packet));

        ++packet.version;
        TEST_ASSERT_FALSE(NodeStatusCodec::Decode(
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet),
            source.macAddress,
            decoded));
    }

    /**
     * @brief 低ID remote TAGの有効なmaster宣言を待つことを検証します。
     */
    void TestRemoteMasterDeclarationWait()
    {
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(MakeStatus(5, EnRunMode::Tag, 5));
        broadcast.PutNode(MakeStatus(1, EnRunMode::Tag, 1), 1000);

        TagMasterCoordinator coordinator(broadcast);
        coordinator.Begin(1000);
        coordinator.Update(1500);
        TEST_ASSERT_TRUE(coordinator.IsElectionComplete());
        TEST_ASSERT_FALSE(coordinator.HasMaster());
        TEST_ASSERT_FALSE(coordinator.IsSelfMaster());

        TagMasterChange change{};
        TEST_ASSERT_FALSE(coordinator.TryTakeMasterChange(change));
        coordinator.Update(1501);
        TEST_ASSERT_FALSE(coordinator.HasMaster());
        TEST_ASSERT_FALSE(coordinator.TryTakeMasterChange(change));

        broadcast.PutNode(
            MakeStatus(1, EnRunMode::Tag, 1, true, 0),
            1501);
        coordinator.Update(1501);
        TEST_ASSERT_FALSE(coordinator.HasMaster());
        TEST_ASSERT_FALSE(coordinator.TryTakeMasterChange(change));

        broadcast.PutNode(
            MakeStatus(1, EnRunMode::Tag, 1, true, 42),
            1501);
        coordinator.Update(1501);
        TEST_ASSERT_TRUE(coordinator.HasMaster());
        TEST_ASSERT_FALSE(coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(1, coordinator.GetMaster().nodeID);
        TEST_ASSERT_EQUAL_UINT32(42, coordinator.GetMaster().sessionId);
        TEST_ASSERT_TRUE(coordinator.TryTakeMasterChange(change));
        TEST_ASSERT_FALSE(change.previousMaster.isValid);
        TEST_ASSERT_TRUE(change.currentMaster.isValid);
        TEST_ASSERT_EQUAL_UINT8(1, change.currentMaster.nodeID);
        TEST_ASSERT_EQUAL_UINT32(42, change.currentMaster.sessionId);
        TEST_ASSERT_FALSE(coordinator.TryTakeMasterChange(change));

        coordinator.Update(1502);
        TEST_ASSERT_FALSE(coordinator.TryTakeMasterChange(change));
    }

    /**
     * @brief 起動待ち、最小TAG選出、失効とsession生成を検証します。
     */
    void TestMasterElection()
    {
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(MakeStatus(5, EnRunMode::Tag, 5));
        broadcast.PutNode(MakeStatus(9, EnRunMode::Tag, 9), 1000);
        broadcast.PutNode(
            MakeStatus(1, EnRunMode::Tag, 1, true, 42),
            1000);

        TagMasterCoordinator coordinator(broadcast);
        coordinator.Begin(1000);
        coordinator.Update(1499);
        TEST_ASSERT_FALSE(coordinator.IsElectionComplete());
        TEST_ASSERT_FALSE(coordinator.HasMaster());

        coordinator.Update(1500);
        TEST_ASSERT_TRUE(coordinator.IsElectionComplete());
        TEST_ASSERT_TRUE(coordinator.HasMaster());
        TEST_ASSERT_FALSE(coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(1, coordinator.GetMaster().nodeID);
        TEST_ASSERT_EQUAL_UINT32(42, coordinator.GetMaster().sessionId);

        TagMasterChange change{};
        TEST_ASSERT_TRUE(coordinator.TryTakeMasterChange(change));
        TEST_ASSERT_TRUE(change.requiresStateReset);
        TEST_ASSERT_EQUAL_UINT8(1, change.currentMaster.nodeID);

        nextRandom = 0;
        coordinator.Update(31000);
        TEST_ASSERT_FALSE(coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(1, coordinator.GetMaster().nodeID);

        coordinator.Update(31001);
        TEST_ASSERT_TRUE(coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(5, coordinator.GetMaster().nodeID);
        TEST_ASSERT_NOT_EQUAL(0, coordinator.GetMaster().sessionId);
        TEST_ASSERT_TRUE(coordinator.TryTakeMasterChange(change));
        TEST_ASSERT_EQUAL_UINT8(1, change.previousMaster.nodeID);
        TEST_ASSERT_EQUAL_UINT8(5, change.currentMaster.nodeID);

        broadcast.PutNode(
            MakeStatus(2, EnRunMode::Tag, 2, true, 77),
            32000);
        coordinator.Update(32000);
        TEST_ASSERT_FALSE(coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(2, coordinator.GetMaster().nodeID);
        TEST_ASSERT_EQUAL_UINT32(77, coordinator.GetMaster().sessionId);
    }

    /**
     * @brief 重複ノードIDの候補除外を検証します。
     */
    void TestDuplicateNodeIdExclusion()
    {
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(MakeStatus(5, EnRunMode::Tag, 5));
        broadcast.PutNode(
            MakeStatus(1, EnRunMode::Tag, 1, true, 11),
            0);
        broadcast.PutNode(MakeStatus(1, EnRunMode::Anchor, 2), 0);

        TagMasterCoordinator coordinator(broadcast);
        coordinator.Begin(0);
        coordinator.Update(500);
        TEST_ASSERT_TRUE(coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(5, coordinator.GetMaster().nodeID);
    }

    /**
     * @brief NR-001受信所有境界とin-memory destructive mutationを検証します。
     */
    void TestReceiveBoundarySourceContract()
    {
        const std::string projectPrefix = FindProjectPrefix(
            "src/EspNowTransport.cpp");
        const std::string transportHeader = ReadTextFile(
            projectPrefix + "include/EspNowTransport.h");
        const std::string transportSource = ReadTextFile(
            projectPrefix + "src/EspNowTransport.cpp");
        const std::string broadcastSource = ReadTextFile(
            projectPrefix + "src/EspNowBroadcast.cpp");
        TEST_ASSERT_FALSE(transportHeader.empty());
        TEST_ASSERT_FALSE(transportSource.empty());
        TEST_ASSERT_FALSE(broadcastSource.empty());
        TEST_ASSERT_TRUE(HasValidReceiveBoundary(
            transportHeader,
            transportSource,
            broadcastSource));

        std::string mutatedTransportSource = transportSource;
        std::string peekBlock;
        TEST_ASSERT_TRUE(ExtractFunctionBlock(
            mutatedTransportSource,
            "bool EspNowTransport::PeekReceive(",
            "bool EspNowTransport::ConsumeReceive()",
            peekBlock));
        const size_t peekStart = mutatedTransportSource.find(
            "bool EspNowTransport::PeekReceive(");
        const size_t queuePeek = mutatedTransportSource.find(
            "xQueuePeek(",
            peekStart);
        TEST_ASSERT_NOT_EQUAL(std::string::npos, queuePeek);
        mutatedTransportSource.replace(
            queuePeek,
            strlen("xQueuePeek"),
            "xQueueReceive");
        TEST_ASSERT_FALSE(HasValidReceiveBoundary(
            transportHeader,
            mutatedTransportSource,
            broadcastSource));
    }
}

/**
 * @brief Unity test前の初期化を行います。
 */
void setUp()
{
}

/**
 * @brief Unity test後の後処理を行います。
 */
void tearDown()
{
}

/**
 * @brief native test用の疑似乱数を返します。
 *
 * @return 設定済みの32bit疑似乱数
 */
extern "C" uint32_t esp_random()
{
    return nextRandom;
}

/**
 * @brief T-003のPlatformIO native test suiteを実行します。
 *
 * @return Unity test結果
 */
int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestNodeStatusCodec);
    RUN_TEST(TestMasterElection);
    RUN_TEST(TestRemoteMasterDeclarationWait);
    RUN_TEST(TestDuplicateNodeIdExclusion);
    RUN_TEST(TestReceiveBoundarySourceContract);
    return UNITY_END();
}
