#include <unity.h>

#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "EspNowTransport.h"
#include "NodeStatus.h"
#include "NtpTimeProtocolCodec.h"
#include "NtpTimeSynchronizer.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"
#include "SequentialRangingProtocolCodec.h"
#include "TagMasterCoordinator.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    uint64_t g_nowUs = 0;
    uint32_t g_nextRandom = 100;

    /**
     * @brief 1ノード分のproduction状態機械とfake依存をまとめます。
     */
    class IntegrationNode
    {
    public:
        /**
         * @brief 指定NodeStatusを自ノードとして統合対象を生成します。
         *
         * @param status 自ノード状態
         */
        explicit IntegrationNode(const NodeStatus& status)
            : m_coordinator(m_broadcast),
              m_synchronizer(
                  m_transport,
                  m_broadcast,
                  m_coordinator,
                  m_config,
                  GetIntegrationTimeUs),
              m_controller(
                  m_transport,
                  m_broadcast,
                  m_coordinator,
                  m_synchronizer,
                  m_ryuw122,
                  m_codec,
                  GetIntegrationTimeUs)
        {
            m_broadcast.SetLocalStatus(status);
        }

        EspNowTransport m_transport;
        EspNowBroadcast m_broadcast;
        TagMasterCoordinator m_coordinator;
        ConfigRuntime m_config;
        NtpTimeSynchronizer m_synchronizer;
        Ryuw122Controller m_ryuw122;
        SequentialRangingProtocolCodec m_codec;
        SequentialRangingController m_controller;
    };

    /**
     * @brief node IDとroleに対応するtest用NodeStatusを生成します。
     *
     * @param nodeId ノードID
     * @param mode 動作モード
     * @return 生成したNodeStatus
     */
    NodeStatus MakeNode(uint8_t nodeId, EnRunMode mode)
    {
        NodeStatus status{};
        status.nodeID = nodeId;
        status.mode = mode;
        status.macAddress[0] = 0x02;
        status.macAddress[5] = nodeId;
        snprintf(
            status.uwbAddress,
            sizeof(status.uwbAddress),
            mode == EnRunMode::Tag ? "T%07u" : "A%07u",
            static_cast<unsigned int>(nodeId));
        return status;
    }

    /**
     * @brief 各broadcastへ自ノード以外の最新NodeStatusを登録します。
     *
     * @param nodes 統合対象ノード一覧
     * @param nodeCount ノード数
     * @param lastSeenMs 登録する最終受信時刻
     */
    void PublishAllStatuses(
        IntegrationNode* nodes[],
        size_t nodeCount,
        uint32_t lastSeenMs)
    {
        for (size_t destinationIndex = 0;
             destinationIndex < nodeCount;
             ++destinationIndex)
        {
            for (size_t sourceIndex = 0;
                 sourceIndex < nodeCount;
                 ++sourceIndex)
            {
                if (sourceIndex == destinationIndex)
                {
                    continue;
                }
                nodes[destinationIndex]->m_broadcast.PutNode(
                    nodes[sourceIndex]->m_broadcast.GetLocalStatus(),
                    lastSeenMs);
            }
        }
    }

    /**
     * @brief 選出済みマスター状態を全remote broadcastへ反映します。
     *
     * @param masterNode マスターTAGノード
     * @param nodes 統合対象ノード一覧
     * @param nodeCount ノード数
     * @param lastSeenMs 登録する最終受信時刻
     */
    void PublishMasterStatus(
        IntegrationNode& masterNode,
        IntegrationNode* nodes[],
        size_t nodeCount,
        uint32_t lastSeenMs)
    {
        const NodeStatus& status = masterNode.m_broadcast.GetLocalStatus();
        for (size_t index = 0; index < nodeCount; ++index)
        {
            if (nodes[index] == &masterNode)
            {
                continue;
            }
            nodes[index]->m_broadcast.PutNode(status, lastSeenMs);
        }
    }

    /**
     * @brief 送信FIFOをMAC宛先の受信FIFOへ配送します。
     *
     * @param nodes 統合対象ノード一覧
     * @param nodeCount ノード数
     */
    void RouteAllPackets(IntegrationNode* nodes[], size_t nodeCount)
    {
        for (size_t sourceIndex = 0; sourceIndex < nodeCount; ++sourceIndex)
        {
            EspNowTestSentPacket sent{};
            while (nodes[sourceIndex]->m_transport.TakeSent(sent))
            {
                bool delivered = false;
                for (size_t destinationIndex = 0;
                     destinationIndex < nodeCount;
                     ++destinationIndex)
                {
                    const NodeStatus& destination =
                        nodes[destinationIndex]->m_broadcast.GetLocalStatus();
                    if (memcmp(
                            destination.macAddress,
                            sent.destinationMac,
                            6) != 0)
                    {
                        continue;
                    }
                    EspNowReceivedPacket received{};
                    memcpy(
                        received.sourceMac,
                        nodes[sourceIndex]->m_broadcast.GetLocalStatus().macAddress,
                        6);
                    memcpy(received.destinationMac, sent.destinationMac, 6);
                    received.rssi = -45;
                    received.channel = 6;
                    received.receivedTimestampUs =
                        static_cast<uint32_t>(g_nowUs + 5U);
                    received.payloadLength = sent.payloadLength;
                    received.hasRxControl = true;
                    memcpy(received.payload, sent.payload, sent.payloadLength);
                    nodes[destinationIndex]->m_transport.PushReceived(received);
                    delivered = true;
                    break;
                }
                TEST_ASSERT_TRUE(delivered);
            }
        }
    }

    /**
     * @brief 全ノードのproduction NTP状態機械を同期完了まで駆動します。
     *
     * @param nodes 統合対象ノード一覧
     * @param nodeCount ノード数
     */
    void DriveSynchronization(
        IntegrationNode* nodes[],
        size_t nodeCount)
    {
        bool completed = false;
        for (size_t iteration = 0; iteration < 256U; ++iteration)
        {
            for (size_t index = 0; index < nodeCount; ++index)
            {
                nodes[index]->m_synchronizer.Update();
            }
            RouteAllPackets(nodes, nodeCount);
            g_nowUs += 10U;
            completed = true;
            for (size_t index = 0; index < nodeCount; ++index)
            {
                completed = completed &&
                    nodes[index]->m_synchronizer.IsSynchronizationComplete();
            }
            if (completed)
            {
                break;
            }
        }
        TEST_ASSERT_TRUE(completed);
    }

    /**
     * @brief 全ノードの同期・逐次測距状態機械を1回駆動します。
     *
     * @param nodes 統合対象ノード一覧
     * @param nodeCount ノード数
     */
    void DriveRangingTick(IntegrationNode* nodes[], size_t nodeCount)
    {
        for (size_t index = 0; index < nodeCount; ++index)
        {
            nodes[index]->m_synchronizer.Update();
            nodes[index]->m_controller.Update();
        }
        RouteAllPackets(nodes, nodeCount);
        g_nowUs += 100U;
    }

    /**
     * @brief 3 ANCHOR×2 TAGをproduction layer直結で統合検証します。
     */
    void TestProductionIntegratedThreeAnchorTwoTagLifecycle()
    {
        IntegrationNode master(MakeNode(1, EnRunMode::Tag));
        IntegrationNode follower(MakeNode(2, EnRunMode::Tag));
        IntegrationNode anchorOne(MakeNode(10, EnRunMode::Anchor));
        IntegrationNode anchorTwo(MakeNode(20, EnRunMode::Anchor));
        IntegrationNode anchorThree(MakeNode(30, EnRunMode::Anchor));
        IntegrationNode* nodes[] = {
            &master, &follower, &anchorOne, &anchorTwo, &anchorThree};
        constexpr size_t NodeCount = sizeof(nodes) / sizeof(nodes[0]);

        PublishAllStatuses(nodes, NodeCount, 0);
        for (IntegrationNode* node : nodes)
        {
            node->m_coordinator.Begin(0);
            node->m_coordinator.Update(500);
        }
        TEST_ASSERT_TRUE(master.m_coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(1, master.m_coordinator.GetMaster().nodeID);
        PublishMasterStatus(master, nodes, NodeCount, 500);
        for (size_t index = 1; index < NodeCount; ++index)
        {
            nodes[index]->m_coordinator.Update(501);
            TEST_ASSERT_TRUE(nodes[index]->m_coordinator.HasMaster());
            TEST_ASSERT_FALSE(nodes[index]->m_coordinator.IsSelfMaster());
        }

        g_nowUs = 501000U;
        DriveSynchronization(nodes, NodeCount);
        NodeTimeSynchronization anchorSynchronization{};
        TEST_ASSERT_TRUE(anchorOne.m_synchronizer.TryGetNodeSynchronization(
            10,
            anchorSynchronization));
        uint64_t anchorMasterTimeUs = 0;
        TEST_ASSERT_TRUE(anchorOne.m_synchronizer.TryConvertLocalTimeToMaster(
            static_cast<uint32_t>(g_nowUs),
            anchorMasterTimeUs));

        for (IntegrationNode* node : nodes)
        {
            node->m_controller.Begin();
        }

        const uint8_t expectedAnchorIds[] = {10, 10, 20, 20, 30, 30};
        const uint8_t expectedTagIds[] = {1, 2, 1, 2, 1, 2};
        size_t measurementCount = 0;
        SequentialRangeRoundSummary firstSummary{};
        bool firstRoundCompleted = false;
        for (size_t iteration = 0; iteration < 256U; ++iteration)
        {
            DriveRangingTick(nodes, NodeCount);
            TimedRangeMeasurement measurement{};
            while (master.m_controller.TryTakeMeasurement(measurement))
            {
                TEST_ASSERT_LESS_THAN_UINT32(6U, measurementCount);
                TEST_ASSERT_EQUAL_UINT8(
                    expectedAnchorIds[measurementCount],
                    measurement.anchorId);
                TEST_ASSERT_EQUAL_UINT8(
                    expectedTagIds[measurementCount],
                    measurement.tagId);
                TEST_ASSERT_EQUAL_UINT8(
                    static_cast<uint8_t>(EnTimeQuality::Synchronized),
                    static_cast<uint8_t>(measurement.timeQuality));
                TEST_ASSERT_TRUE(
                    measurement.rangingCompletedMasterTimeUs > 0U);
                ++measurementCount;
            }
            if (master.m_controller.TryTakeCompletedRound(firstSummary))
            {
                firstRoundCompleted = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(firstRoundCompleted);
        TEST_ASSERT_EQUAL_UINT32(6U, measurementCount);
        TEST_ASSERT_EQUAL_UINT8(6, firstSummary.expectedMeasurementCount);
        TEST_ASSERT_EQUAL_UINT8(6, firstSummary.receivedMeasurementCount);
        TEST_ASSERT_FALSE(firstSummary.timedOut);
        TEST_ASSERT_EQUAL_UINT32(2U, anchorOne.m_ryuw122.GetStartCount());
        TEST_ASSERT_EQUAL_UINT32(2U, anchorTwo.m_ryuw122.GetStartCount());
        TEST_ASSERT_EQUAL_UINT32(2U, anchorThree.m_ryuw122.GetStartCount());
        TEST_ASSERT_EQUAL_STRING(
            follower.m_broadcast.GetLocalStatus().uwbAddress,
            anchorOne.m_ryuw122.GetStartedAddress(1));

        anchorOne.m_ryuw122.SetAutoComplete(false);
        anchorTwo.m_ryuw122.SetAutoComplete(false);
        anchorThree.m_ryuw122.SetAutoComplete(false);
        DriveRangingTick(nodes, NodeCount);
        g_nowUs += 2100000U;
        master.m_synchronizer.Update();
        master.m_controller.Update();
        SequentialRangeRoundSummary timeoutSummary{};
        TEST_ASSERT_TRUE(master.m_controller.TryTakeCompletedRound(
            timeoutSummary));
        TEST_ASSERT_EQUAL_UINT32(2U, timeoutSummary.roundId);
        TEST_ASSERT_TRUE(timeoutSummary.timedOut);
        TEST_ASSERT_EQUAL_UINT8(0, timeoutSummary.receivedMeasurementCount);

        const uint32_t resetBeforeChange =
            master.m_controller.GetResetGeneration();
        NodeStatus lowerMaster = MakeNode(0, EnRunMode::Tag);
        lowerMaster.isMaster = true;
        lowerMaster.sessionId = 900;
        const uint32_t lowerMasterSeenMs =
            static_cast<uint32_t>(g_nowUs / 1000U);
        for (IntegrationNode* node : nodes)
        {
            node->m_broadcast.PutNode(lowerMaster, lowerMasterSeenMs);
            node->m_coordinator.Update(lowerMasterSeenMs);
            node->m_synchronizer.Update();
            node->m_controller.Update();
        }
        TEST_ASSERT_FALSE(master.m_coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT8(0, master.m_coordinator.GetMaster().nodeID);
        TEST_ASSERT_EQUAL_UINT32(
            resetBeforeChange + 1U,
            master.m_controller.GetResetGeneration());
        TEST_ASSERT_FALSE(master.m_synchronizer.IsSynchronizationComplete());
        anchorOne.m_ryuw122.SetAutoComplete(true);
        anchorTwo.m_ryuw122.SetAutoComplete(true);
        anchorThree.m_ryuw122.SetAutoComplete(true);
        anchorOne.m_controller.Update();
        anchorTwo.m_controller.Update();
        anchorThree.m_controller.Update();

        g_nowUs = static_cast<uint64_t>(lowerMasterSeenMs + 30001U) * 1000U;
        const uint32_t recoveredAtMs =
            static_cast<uint32_t>(g_nowUs / 1000U);
        PublishAllStatuses(nodes, NodeCount, recoveredAtMs);
        g_nextRandom = 200;
        master.m_coordinator.Update(recoveredAtMs);
        TEST_ASSERT_TRUE(master.m_coordinator.IsSelfMaster());
        TEST_ASSERT_EQUAL_UINT32(200U, master.m_coordinator.GetMaster().sessionId);
        PublishMasterStatus(master, nodes, NodeCount, recoveredAtMs);
        for (size_t index = 1; index < NodeCount; ++index)
        {
            nodes[index]->m_coordinator.Update(recoveredAtMs);
        }
        for (IntegrationNode* node : nodes)
        {
            node->m_transport.ClearPackets();
            node->m_synchronizer.Update();
            node->m_controller.Update();
        }
        TEST_ASSERT_EQUAL_UINT32(
            resetBeforeChange + 2U,
            master.m_controller.GetResetGeneration());
        TEST_ASSERT_FALSE(master.m_synchronizer.IsSynchronizationComplete());

        DriveSynchronization(nodes, NodeCount);
        const size_t startsBeforeRestart = anchorOne.m_ryuw122.GetStartCount();
        for (size_t iteration = 0; iteration < 8U; ++iteration)
        {
            DriveRangingTick(nodes, NodeCount);
            if (anchorOne.m_ryuw122.GetStartCount() > startsBeforeRestart)
            {
                break;
            }
        }
        TEST_ASSERT_TRUE(
            anchorOne.m_ryuw122.GetStartCount() > startsBeforeRestart);

        TEST_ASSERT_EQUAL_UINT32(29U, sizeof(NodeStatusWirePacket));
        TEST_ASSERT_EQUAL_UINT32(34U, sizeof(NtpSyncCommitPacket));
        TEST_ASSERT_EQUAL_UINT32(45U, sizeof(RangeControlPacket));
        TEST_ASSERT_EQUAL_UINT32(117U, sizeof(RangeMeasurementPacket));
        TEST_ASSERT_EQUAL_UINT32(58U, sizeof(RangeRoundCompletePacket));
        TEST_ASSERT_TRUE(sizeof(RangeMeasurementPacket) <= 250U);
    }
}

/**
 * @brief native統合テストの疑似マイクロ秒時刻を返します。
 *
 * @return 現在の疑似マイクロ秒時刻
 */
uint64_t GetIntegrationTimeUs()
{
    return g_nowUs;
}

/**
 * @brief 各Unity test前に疑似時刻と乱数を初期化します。
 */
void setUp()
{
    g_nowUs = 0;
    g_nextRandom = 100;
}

/**
 * @brief Unity test後の追加処理はありません。
 */
void tearDown()
{
}

/**
 * @brief production session生成へ疑似乱数を返します。
 *
 * @return 設定済みの32bit疑似乱数
 */
extern "C" uint32_t esp_random()
{
    return g_nextRandom;
}

/**
 * @brief production既定時刻取得へ疑似時刻を返します。
 *
 * @return 現在の疑似マイクロ秒時刻
 */
extern "C" int64_t esp_timer_get_time()
{
    return static_cast<int64_t>(g_nowUs);
}

/**
 * @brief T-009のPlatformIO native統合テストを実行します。
 *
 * @return Unity test結果
 */
int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestProductionIntegratedThreeAnchorTwoTagLifecycle);
    return UNITY_END();
}
