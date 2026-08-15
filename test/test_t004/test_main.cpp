#ifdef T004_TRANSPORT_TEST

#include <unity.h>

#include "../../include/EspNowTransport.h"
#include "EspNowTestRuntime.h"

#include <cstdint>

namespace
{
    /**
     * @brief ESP-NOW受信時刻がrx_ctrl時計ではなくESP Timer時計になることを検証します。
     */
    void TestReceiveTimestampUsesEspTimerClockDomain()
    {
        EspNowTransport transport;
        TEST_ASSERT_TRUE(transport.Begin(6, false));

        const uint8_t sourceMac[ESP_NOW_ETH_ALEN] = {0x02, 0, 0, 0, 0, 1};
        const uint8_t destinationMac[ESP_NOW_ETH_ALEN] = {
            0x02, 0, 0, 0, 0, 2};
        const uint8_t payload[] = {1, 2, 3};
        wifi_pkt_rx_ctrl_t rxControl{};
        rxControl.rssi = -48;
        rxControl.channel = 6;
        rxControl.timestamp = 1234;
        esp_now_recv_info_t info{};
        info.src_addr = sourceMac;
        info.des_addr = destinationMac;
        info.rx_ctrl = &rxControl;
        SetEspNowTestTimeUs(UINT64_C(0x100001234));

        TEST_ASSERT_TRUE(InvokeEspNowTestReceive(
            info,
            payload,
            sizeof(payload)));
        EspNowReceivedPacket received{};
        TEST_ASSERT_TRUE(transport.TryReceive(received));
        TEST_ASSERT_EQUAL_UINT32(0x1234, received.receivedTimestampUs);
        TEST_ASSERT_NOT_EQUAL(rxControl.timestamp, received.receivedTimestampUs);
        TEST_ASSERT_EQUAL_INT8(-48, received.rssi);
        TEST_ASSERT_EQUAL_UINT8(6, received.channel);
        TEST_ASSERT_TRUE(received.hasRxControl);
    }

    /**
     * @brief rx_ctrl欠落時もESP Timer時計を受信時刻へ保存することを検証します。
     */
    void TestReceiveTimestampWithoutRxControlUsesEspTimer()
    {
        EspNowTransport transport;
        TEST_ASSERT_TRUE(transport.Begin(1, false));

        const uint8_t sourceMac[ESP_NOW_ETH_ALEN] = {0x02, 0, 0, 0, 0, 3};
        const uint8_t destinationMac[ESP_NOW_ETH_ALEN] = {
            0x02, 0, 0, 0, 0, 4};
        const uint8_t payload[] = {9};
        esp_now_recv_info_t info{};
        info.src_addr = sourceMac;
        info.des_addr = destinationMac;
        SetEspNowTestTimeUs(987654);

        TEST_ASSERT_TRUE(InvokeEspNowTestReceive(
            info,
            payload,
            sizeof(payload)));
        EspNowReceivedPacket received{};
        TEST_ASSERT_TRUE(transport.TryReceive(received));
        TEST_ASSERT_EQUAL_UINT32(987654, received.receivedTimestampUs);
        TEST_ASSERT_FALSE(received.hasRxControl);
    }
}

/**
 * @brief 各transport test前の追加処理はありません。
 */
void setUp()
{
}

/**
 * @brief 各transport test後の追加処理はありません。
 */
void tearDown()
{
}

/**
 * @brief production EspNowTransportを直接結合したnative testを実行します。
 *
 * @return Unity test結果
 */
int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestReceiveTimestampUsesEspTimerClockDomain);
    RUN_TEST(TestReceiveTimestampWithoutRxControlUsesEspTimer);
    return UNITY_END();
}

#else

#include <unity.h>

#include "ConfigRuntime.h"
#include "EspNowBroadcast.h"
#include "EspNowTransport.h"
#include "NtpTimeProtocolCodec.h"
#include "NtpTimeSynchronizer.h"
#include "TagMasterCoordinator.h"

#include <cstdint>
#include <cstring>

namespace
{
    uint64_t fakeTimeUs = 0;

    /**
     * @brief test用の単調増加マイクロ秒時刻を返します。
     *
     * @return 設定済みtest時刻
     */
    uint64_t GetFakeTimeUs()
    {
        return fakeTimeUs;
    }

    /**
     * @brief test用NodeStatusを生成します。
     *
     * @param nodeId ノードID
     * @param mode 動作モード
     * @param macTail MACアドレス末尾
     * @return 生成したNodeStatus
     */
    NodeStatus MakeStatus(
        uint8_t nodeId,
        EnRunMode mode,
        uint8_t macTail)
    {
        NodeStatus status{};
        status.nodeID = nodeId;
        status.mode = mode;
        status.macAddress[0] = 0x02;
        status.macAddress[5] = macTail;
        memcpy(status.uwbAddress, "T0000000", 9);
        return status;
    }

    /**
     * @brief NodeStatusからtest用マスター識別情報を生成します。
     *
     * @param status マスターTAGの状態
     * @param sessionId マスターセッションID
     * @return 生成したマスター識別情報
     */
    TagMasterIdentity MakeMasterIdentity(
        const NodeStatus& status,
        uint32_t sessionId)
    {
        TagMasterIdentity identity{};
        identity.isValid = true;
        identity.nodeID = status.nodeID;
        memcpy(
            identity.macAddress.data(),
            status.macAddress,
            identity.macAddress.size());
        identity.sessionId = sessionId;
        return identity;
    }

    /**
     * @brief 送信済み同期要求に対応する受信packetを生成します。
     *
     * @param request 送信済み同期要求
     * @param sourceStatus 応答ノード状態
     * @param destinationStatus マスターTAG状態
     * @param oneWayDelayUs 片方向遅延
     * @param processingUs 対象ノード内処理時間
     * @param offsetUs 対象時計からマスター時計へのoffset
     * @param receiveTimestampAvailable remote側rx_ctrl timestamp有無
     * @param remotePowerSaveEnabled remote側Wi-Fi省電力設定
     * @return 生成した受信packet
     */
    EspNowReceivedPacket MakeResponse(
        const NtpSyncRequestPacket& request,
        const NodeStatus& sourceStatus,
        const NodeStatus& destinationStatus,
        uint32_t oneWayDelayUs,
        uint32_t processingUs,
        int32_t offsetUs,
        bool receiveTimestampAvailable = true,
        bool remotePowerSaveEnabled = false)
    {
        const uint32_t t2 = request.t1 + oneWayDelayUs +
            static_cast<uint32_t>(offsetUs);
        const uint32_t t3 = t2 + processingUs;
        const uint32_t t4 = request.t1 +
            (oneWayDelayUs * 2U) + processingUs;
        NtpSyncResponsePacket response{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::EncodeResponse(
            request.header.sessionId,
            request.header.sequence,
            request.targetNodeId,
            request.t1,
            t2,
            t3,
            receiveTimestampAvailable,
            remotePowerSaveEnabled,
            response));

        EspNowReceivedPacket received{};
        memcpy(received.sourceMac, sourceStatus.macAddress, 6);
        memcpy(received.destinationMac, destinationStatus.macAddress, 6);
        received.rssi = -42;
        received.channel = 1;
        received.receivedTimestampUs = t4;
        received.payloadLength = sizeof(response);
        received.hasRxControl = true;
        memcpy(received.payload, &response, sizeof(response));
        fakeTimeUs += (oneWayDelayUs * 2U) + processingUs;
        return received;
    }

    /**
     * @brief 最新送信要求へ応答して同期状態機械を1サンプル進めます。
     *
     * @param synchronizer 進める同期状態機械
     * @param transport test用transport
     * @param masterStatus マスターTAG状態
     * @param remoteStatus 対象ノード状態
     * @param oneWayDelayUs 片方向遅延
     * @param offsetUs 対象時計からマスター時計へのoffset
     * @param receiveTimestampAvailable remote側rx_ctrl timestamp有無
     * @param remotePowerSaveEnabled remote側Wi-Fi省電力設定
     */
    void RespondToLatestRequest(
        NtpTimeSynchronizer& synchronizer,
        EspNowTransport& transport,
        const NodeStatus& masterStatus,
        const NodeStatus& remoteStatus,
        uint32_t oneWayDelayUs,
        int32_t offsetUs,
        bool receiveTimestampAvailable = true,
        bool remotePowerSaveEnabled = false)
    {
        const auto& sent = transport.GetSentPackets();
        TEST_ASSERT_FALSE(sent.empty());
        NtpSyncRequestPacket request{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
            sent.back().payload.data(),
            sent.back().payload.size(),
            request));
        transport.PushReceived(MakeResponse(
            request,
            remoteStatus,
            masterStatus,
            oneWayDelayUs,
            10,
            offsetUs,
            receiveTimestampAvailable,
            remotePowerSaveEnabled));
        synchronizer.Update();
    }

    /**
     * @brief 非マスターノードへtest用SyncCommitを受信させます。
     *
     * @param synchronizer 更新する同期状態機械
     * @param transport test用transport
     * @param masterStatus マスターTAG状態
     * @param targetStatus 同期対象ノード状態
     * @param sessionId マスターセッションID
     * @param nodeMinusMasterUs 対象ノード時計からmaster時計へのoffset
     * @param synchronizedAtMasterTimeUs commit時のmaster 64bit時刻
     */
    void ApplyNodeCommit(
        NtpTimeSynchronizer& synchronizer,
        EspNowTransport& transport,
        const NodeStatus& masterStatus,
        const NodeStatus& targetStatus,
        uint32_t sessionId,
        int64_t nodeMinusMasterUs,
        uint64_t synchronizedAtMasterTimeUs)
    {
        NtpSyncCommitPacket commit{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::EncodeCommit(
            sessionId,
            4,
            targetStatus.nodeID,
            nodeMinusMasterUs,
            10,
            EnTimeQuality::Synchronized,
            synchronizedAtMasterTimeUs,
            commit));
        EspNowReceivedPacket received{};
        memcpy(received.sourceMac, masterStatus.macAddress, 6);
        memcpy(received.destinationMac, targetStatus.macAddress, 6);
        received.channel = 1;
        received.receivedTimestampUs = static_cast<uint32_t>(fakeTimeUs);
        received.payloadLength = sizeof(commit);
        received.hasRxControl = true;
        memcpy(received.payload, &commit, sizeof(commit));
        transport.PushReceived(received);
        synchronizer.Update();
    }

    /**
     * @brief 現在対象の3サンプル同期とcommit処理を完了します。
     *
     * @param synchronizer 進める同期状態機械
     * @param transport test用transport
     * @param masterStatus マスターTAG状態
     * @param remoteStatus 対象ノード状態
     * @param offsetUs 対象時計からマスター時計へのoffset
     */
    void CompleteCurrentTarget(
        NtpTimeSynchronizer& synchronizer,
        EspNowTransport& transport,
        const NodeStatus& masterStatus,
        const NodeStatus& remoteStatus,
        int32_t offsetUs)
    {
        for (size_t index = 0;
             index < NtpTimeSynchronizer::m_sampleCountPerNode;
             ++index)
        {
            RespondToLatestRequest(
                synchronizer,
                transport,
                masterStatus,
                remoteStatus,
                5,
                offsetUs);
        }
        synchronizer.Update();
        synchronizer.Update();
    }

    /**
     * @brief 送信履歴に含まれる指定対象の同期要求件数を数えます。
     *
     * @param transport test用transport
     * @param targetNodeId 対象ノードID
     * @return 指定対象へ送った同期要求件数
     */
    size_t CountRequestsForTarget(
        const EspNowTransport& transport,
        uint8_t targetNodeId)
    {
        size_t count = 0;
        for (const auto& sent : transport.GetSentPackets())
        {
            NtpSyncRequestPacket request{};
            if (NtpTimeProtocolCodec::DecodeRequest(
                    sent.payload.data(),
                    sent.payload.size(),
                    request) &&
                request.targetNodeId == targetNodeId)
            {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief NTP固定wire codecの往復と破損拒否を検証します。
     */
    void TestPacketCodecValidation()
    {
        const uint8_t masterMac[6] = {0x02, 1, 2, 3, 4, 5};
        NtpSyncRequestPacket request{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::EncodeRequest(
            77, 9, 1, masterMac, 4, 123, request));
        TEST_ASSERT_EQUAL_UINT32(24, sizeof(request));
        NtpSyncRequestPacket decodedRequest{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request),
            decodedRequest));
        TEST_ASSERT_EQUAL_UINT32(77, decodedRequest.header.sessionId);
        TEST_ASSERT_EQUAL_UINT32(9, decodedRequest.header.sequence);
        TEST_ASSERT_EQUAL_UINT8(4, decodedRequest.targetNodeId);
        TEST_ASSERT_EQUAL_UINT32(123, decodedRequest.t1);

        ++request.header.version;
        TEST_ASSERT_FALSE(NtpTimeProtocolCodec::DecodeRequest(
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request),
            decodedRequest));
        --request.header.version;
        request.header.sessionId = 0;
        TEST_ASSERT_FALSE(NtpTimeProtocolCodec::DecodeRequest(
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request),
            decodedRequest));
        TEST_ASSERT_FALSE(NtpTimeProtocolCodec::DecodeRequest(
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request) - 1,
            decodedRequest));

        NtpSyncResponsePacket response{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::EncodeResponse(
            77, 9, 4, 123, 200, 210, true, false, response));
        response.receiveTimestampAvailable = 2;
        NtpSyncResponsePacket decodedResponse{};
        TEST_ASSERT_FALSE(NtpTimeProtocolCodec::DecodeResponse(
            reinterpret_cast<const uint8_t*>(&response),
            sizeof(response),
            decodedResponse));

        NtpSyncCommitPacket commit{};
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::EncodeCommit(
            77,
            10,
            4,
            25,
            40,
            EnTimeQuality::Synchronized,
            1000,
            commit));
        commit.timeQuality = 255;
        NtpSyncCommitPacket decodedCommit{};
        TEST_ASSERT_FALSE(NtpTimeProtocolCodec::DecodeCommit(
            reinterpret_cast<const uint8_t*>(&commit),
            sizeof(commit),
            decodedCommit));
    }

    /**
     * @brief NTP四時刻のoffsetと往復遅延計算を検証します。
     */
    void TestOffsetAndRoundTripCalculation()
    {
        NtpTimeSample sample{};
        TEST_ASSERT_TRUE(NtpTimeSynchronizer::CalculateSample(
            1000,
            1250,
            1260,
            1070,
            sample));
        TEST_ASSERT_EQUAL_INT64(220, sample.nodeMinusMasterUs);
        TEST_ASSERT_EQUAL_UINT32(60, sample.roundTripUs);

        TEST_ASSERT_FALSE(NtpTimeSynchronizer::CalculateSample(
            1000,
            900,
            1100,
            1050,
            sample));
    }

    /**
     * @brief 32bit折り返し計算と参照時刻近傍への拡張を検証します。
     */
    void TestTimestampWrapAndExtension()
    {
        NtpTimeSample sample{};
        TEST_ASSERT_TRUE(NtpTimeSynchronizer::CalculateSample(
            0xfffffff0U,
            9U,
            19U,
            4U,
            sample));
        TEST_ASSERT_EQUAL_INT64(20, sample.nodeMinusMasterUs);
        TEST_ASSERT_EQUAL_UINT32(10, sample.roundTripUs);

        const uint64_t reference = (uint64_t{3} << 32) + 16U;
        TEST_ASSERT_EQUAL_UINT64(
            (uint64_t{3} << 32) + 4U,
            NtpTimeSynchronizer::ExtendTimestampNear(4U, reference));
        TEST_ASSERT_EQUAL_INT64(
            20,
            NtpTimeSynchronizer::ModuloDifference(4U, 0xfffffff0U));
    }

    /**
     * @brief 3サンプルから最小往復遅延を採用することを検証します。
     */
    void TestBestSampleSelection()
    {
        NtpTimeSample samples[3]{};
        samples[0].isValid = true;
        samples[0].roundTripUs = 80;
        samples[0].nodeMinusMasterUs = 10;
        samples[1].isValid = true;
        samples[1].roundTripUs = 20;
        samples[1].nodeMinusMasterUs = 30;
        samples[2].isValid = true;
        samples[2].roundTripUs = 40;
        samples[2].nodeMinusMasterUs = 50;
        NtpTimeSample selected{};
        TEST_ASSERT_TRUE(NtpTimeSynchronizer::SelectBestSample(
            samples,
            3,
            selected));
        TEST_ASSERT_EQUAL_UINT32(20, selected.roundTripUs);
        TEST_ASSERT_EQUAL_INT64(30, selected.nodeMinusMasterUs);
    }

    /**
     * @brief rx_ctrl欠落とWi-Fi省電力を時刻品質へ反映することを検証します。
     */
    void TestTimeQuality()
    {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnTimeQuality::Synchronized),
            static_cast<uint8_t>(NtpTimeSynchronizer::ResolveTimeQuality(
                false, false, true, true)));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnTimeQuality::PowerSaveEnabled),
            static_cast<uint8_t>(NtpTimeSynchronizer::ResolveTimeQuality(
                true, false, true, true)));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(EnTimeQuality::ReceiveTimestampUnavailable),
            static_cast<uint8_t>(NtpTimeSynchronizer::ResolveTimeQuality(
                true, true, false, true)));
    }

    /**
     * @brief self masterがprovider現在値をマスター基準現在時刻として返すことを検証します。
     */
    void TestCurrentMasterTimeForSelfMaster()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1234567;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        uint64_t currentMasterTimeUs = 999;
        TEST_ASSERT_FALSE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(999, currentMasterTimeUs);
        synchronizer.Update();
        TEST_ASSERT_TRUE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(1234567, currentMasterTimeUs);

        fakeTimeUs = 2234567;
        TEST_ASSERT_TRUE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(2234567, currentMasterTimeUs);

        coordinator.SetMaster(TagMasterIdentity{}, false);
        synchronizer.Update();
        currentMasterTimeUs = 888;
        TEST_ASSERT_FALSE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(888, currentMasterTimeUs);
    }

    /**
     * @brief followerが同期済みローカル現在値をマスター基準現在時刻へ変換することを検証します。
     */
    void TestCurrentMasterTimeForFollower()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus followerStatus = MakeStatus(4, EnRunMode::Tag, 4);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(followerStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            false);
        ConfigRuntime config;
        fakeTimeUs = 5201;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        uint64_t currentMasterTimeUs = 999;
        TEST_ASSERT_FALSE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(999, currentMasterTimeUs);
        ApplyNodeCommit(
            synchronizer,
            transport,
            masterStatus,
            followerStatus,
            77,
            200,
            5000);
        TEST_ASSERT_TRUE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(5001, currentMasterTimeUs);

        fakeTimeUs = 6201;
        TEST_ASSERT_TRUE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(6001, currentMasterTimeUs);

        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 88),
            false);
        synchronizer.Update();
        currentMasterTimeUs = 777;
        TEST_ASSERT_FALSE(synchronizer.TryGetCurrentMasterTime(
            currentMasterTimeUs));
        TEST_ASSERT_EQUAL_UINT64(777, currentMasterTimeUs);
    }

    /**
     * @brief 1ノード3サンプル同期、最小RTT、変換、マスター変更resetを検証します。
     */
    void TestOneNodeSynchronizationFlowAndMasterReset()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus followerStatus = MakeStatus(4, EnRunMode::Tag, 4);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        broadcast.PutNode(followerStatus, 1000);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        TEST_ASSERT_EQUAL_UINT32(1, transport.GetSentPackets().size());
        RespondToLatestRequest(
            synchronizer, transport, masterStatus, followerStatus, 30, 200);
        RespondToLatestRequest(
            synchronizer, transport, masterStatus, followerStatus, 5, 200);
        RespondToLatestRequest(
            synchronizer, transport, masterStatus, followerStatus, 20, 200);
        synchronizer.Update();
        synchronizer.Update();

        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(4, transport.GetSentPackets().size());
        NtpSyncCommitPacket commit{};
        const auto& commitWire = transport.GetSentPackets().back().payload;
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeCommit(
            commitWire.data(),
            commitWire.size(),
            commit));
        TEST_ASSERT_EQUAL_INT64(200, commit.nodeMinusMasterUs);
        TEST_ASSERT_EQUAL_UINT32(10, commit.roundTripUs);

        ++fakeTimeUs;
        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            followerStatus.nodeID,
            synchronization));
        TEST_ASSERT_EQUAL_INT64(200, synchronization.nodeMinusMasterUs);
        TEST_ASSERT_EQUAL_UINT32(10, synchronization.roundTripUs);
        TEST_ASSERT_TRUE(synchronization.synchronizationAgeUs > 0U);
        uint64_t converted = 0;
        TEST_ASSERT_TRUE(synchronizer.TryConvertNodeTimeToMaster(
            followerStatus.nodeID,
            5200,
            5000,
            converted));
        TEST_ASSERT_EQUAL_UINT64(5000, converted);

        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 88),
            true);
        synchronizer.Update();
        TEST_ASSERT_FALSE(synchronizer.TryGetNodeSynchronization(
            followerStatus.nodeID,
            synchronization));
    }

    /**
     * @brief フォロワーTAGがSyncCommitで自ローカル時刻を変換できることを検証します。
     */
    void TestFollowerCommitOffsetAndAge()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus followerStatus = MakeStatus(4, EnRunMode::Tag, 4);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(followerStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            false);
        ConfigRuntime config;
        fakeTimeUs = 5201;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        ApplyNodeCommit(
            synchronizer,
            transport,
            masterStatus,
            followerStatus,
            77,
            200,
            5000);

        uint64_t converted = 0;
        TEST_ASSERT_TRUE(synchronizer.TryConvertLocalTimeToMaster(
            5201,
            converted));
        TEST_ASSERT_EQUAL_UINT64(5001, converted);
        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            followerStatus.nodeID,
            synchronization));
        TEST_ASSERT_EQUAL_UINT64(1, synchronization.synchronizationAgeUs);

        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 88),
            false);
        fakeTimeUs = 4801;
        synchronizer.Update();
        ApplyNodeCommit(
            synchronizer,
            transport,
            masterStatus,
            followerStatus,
            88,
            -200,
            5000);
        TEST_ASSERT_TRUE(synchronizer.TryConvertLocalTimeToMaster(
            4801,
            converted));
        TEST_ASSERT_EQUAL_UINT64(5001, converted);
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            followerStatus.nodeID,
            synchronization));
        TEST_ASSERT_EQUAL_UINT64(1, synchronization.synchronizationAgeUs);
    }

    /**
     * @brief follower時計の32bit wrap前後を同じmaster epochへ変換することを検証します。
     */
    void TestFollowerCommitWrapConversion()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus followerStatus = MakeStatus(4, EnRunMode::Tag, 4);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(followerStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            false);
        ConfigRuntime config;
        const uint64_t masterCommitTimeUs = (uint64_t{1} << 32) - 100U;
        fakeTimeUs = (uint64_t{1} << 32) + 101U;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);
        ApplyNodeCommit(
            synchronizer,
            transport,
            masterStatus,
            followerStatus,
            77,
            200,
            masterCommitTimeUs);

        uint64_t converted = 0;
        TEST_ASSERT_TRUE(synchronizer.TryConvertLocalTimeToMaster(
            101U,
            converted));
        TEST_ASSERT_EQUAL_UINT64(masterCommitTimeUs + 1U, converted);
        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            followerStatus.nodeID,
            synchronization));
        TEST_ASSERT_EQUAL_UINT64(1, synchronization.synchronizationAgeUs);
    }

    /**
     * @brief commitから半epoch超経過後も移動参照で正しいmaster epochを選ぶことを検証します。
     */
    void TestFollowerMovingEpochReference()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus followerStatus = MakeStatus(4, EnRunMode::Tag, 4);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(followerStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            false);
        ConfigRuntime config;
        const uint64_t masterCommitTimeUs =
            (uint64_t{1} << 32) + 5000U;
        const uint64_t localCommitTimeUs =
            (uint64_t{1} << 32) + 5200U;
        const uint64_t elapsedUs = (uint64_t{1} << 31) + 100U;
        fakeTimeUs = localCommitTimeUs;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);
        ApplyNodeCommit(
            synchronizer,
            transport,
            masterStatus,
            followerStatus,
            77,
            200,
            masterCommitTimeUs);

        fakeTimeUs = localCommitTimeUs + elapsedUs;
        uint64_t converted = 0;
        TEST_ASSERT_TRUE(synchronizer.TryConvertLocalTimeToMaster(
            static_cast<uint32_t>(fakeTimeUs),
            converted));
        TEST_ASSERT_EQUAL_UINT64(
            masterCommitTimeUs + elapsedUs,
            converted);
        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            followerStatus.nodeID,
            synchronization));
        TEST_ASSERT_EQUAL_UINT64(
            elapsedUs,
            synchronization.synchronizationAgeUs);
    }

    /**
     * @brief ANCHORがSyncCommitで同期完了しローカル時刻を変換できることを検証します。
     */
    void TestAnchorCommitCompletesSynchronizationAndConversion()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus anchorStatus = MakeStatus(10, EnRunMode::Anchor, 10);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(anchorStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            false);
        ConfigRuntime config;
        fakeTimeUs = 5201;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        TEST_ASSERT_FALSE(synchronizer.IsSynchronizationComplete());
        ApplyNodeCommit(
            synchronizer,
            transport,
            masterStatus,
            anchorStatus,
            77,
            200,
            5000);

        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            anchorStatus.nodeID,
            synchronization));
        TEST_ASSERT_EQUAL_INT64(200, synchronization.nodeMinusMasterUs);
        uint64_t converted = 0;
        TEST_ASSERT_TRUE(synchronizer.TryConvertLocalTimeToMaster(
            5201,
            converted));
        TEST_ASSERT_EQUAL_UINT64(5001, converted);
    }

    /**
     * @brief 初回0件後と同期完了後のlate nodeをID順で一度だけ同期することを検証します。
     */
    void TestLateNodeDiscoveryBeforePeriodicResynchronization()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus anchor3 = MakeStatus(3, EnRunMode::Anchor, 3);
        const NodeStatus follower7 = MakeStatus(7, EnRunMode::Tag, 7);
        const NodeStatus lateAnchor5 = MakeStatus(5, EnRunMode::Anchor, 5);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_TRUE(transport.GetSentPackets().empty());
        broadcast.PutNode(follower7, 1000);
        broadcast.PutNode(anchor3, 1000);
        synchronizer.Update();
        NtpSyncRequestPacket request{};
        const auto& firstWire = transport.GetSentPackets().back().payload;
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
            firstWire.data(), firstWire.size(), request));
        TEST_ASSERT_EQUAL_UINT8(3, request.targetNodeId);
        CompleteCurrentTarget(
            synchronizer, transport, masterStatus, anchor3, 0);
        const auto& secondWire = transport.GetSentPackets().back().payload;
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
            secondWire.data(), secondWire.size(), request));
        TEST_ASSERT_EQUAL_UINT8(7, request.targetNodeId);
        CompleteCurrentTarget(
            synchronizer, transport, masterStatus, follower7, 0);
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());

        broadcast.PutNode(lateAnchor5, 1000);
        synchronizer.Update();
        const auto& lateWire = transport.GetSentPackets().back().payload;
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
            lateWire.data(), lateWire.size(), request));
        TEST_ASSERT_EQUAL_UINT8(5, request.targetNodeId);
        CompleteCurrentTarget(
            synchronizer, transport, masterStatus, lateAnchor5, 0);
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        const size_t sentCount = transport.GetSentPackets().size();
        synchronizer.Update();
        synchronizer.Update();
        TEST_ASSERT_EQUAL_UINT32(
            sentCount,
            transport.GetSentPackets().size());
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 3));
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 5));
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 7));
    }

    /**
     * @brief 全サンプル失敗時に未完了を維持し1秒後に再試行することを検証します。
     */
    void TestFailedTargetRetriesAfterOneSecond()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus anchorStatus = MakeStatus(3, EnRunMode::Anchor, 3);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        broadcast.PutNode(anchorStatus, 1000);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        for (size_t index = 0;
             index < NtpTimeSynchronizer::m_sampleCountPerNode;
             ++index)
        {
            fakeTimeUs += NtpTimeSynchronizer::m_responseTimeoutUs;
            synchronizer.Update();
        }
        TEST_ASSERT_FALSE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 3));

        fakeTimeUs += 999999;
        synchronizer.Update();
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 3));
        fakeTimeUs += 1;
        synchronizer.Update();
        TEST_ASSERT_EQUAL_UINT32(4, CountRequestsForTarget(transport, 3));
        TEST_ASSERT_FALSE(synchronizer.IsSynchronizationComplete());
    }

    /**
     * @brief 正常同期完了から30秒後に全有効ノードを再同期することを検証します。
     */
    void TestSuccessfulSynchronizationResynchronizesAfterThirtySeconds()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus anchorStatus = MakeStatus(3, EnRunMode::Anchor, 3);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        broadcast.PutNode(anchorStatus, 1000);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        CompleteCurrentTarget(
            synchronizer,
            transport,
            masterStatus,
            anchorStatus,
            100);
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        const uint64_t completedAtUs = fakeTimeUs;

        fakeTimeUs = completedAtUs + 29999999;
        synchronizer.Update();
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 3));
        fakeTimeUs += 1;
        synchronizer.Update();
        TEST_ASSERT_FALSE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(4, CountRequestsForTarget(transport, 3));

        NodeTimeSynchronization previousSynchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            anchorStatus.nodeID,
            previousSynchronization));
        TEST_ASSERT_EQUAL_INT64(
            100,
            previousSynchronization.nodeMinusMasterUs);
        CompleteCurrentTarget(
            synchronizer,
            transport,
            masterStatus,
            anchorStatus,
            200);
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(6, CountRequestsForTarget(transport, 3));
        NodeTimeSynchronization updatedSynchronization{};
        TEST_ASSERT_TRUE(synchronizer.TryGetNodeSynchronization(
            anchorStatus.nodeID,
            updatedSynchronization));
        TEST_ASSERT_EQUAL_INT64(
            200,
            updatedSynchronization.nodeMinusMasterUs);
    }

    /**
     * @brief 周期再同期時に消失ノードを対象と同期情報から除外することを検証します。
     */
    void TestPeriodicResynchronizationExcludesMissingNode()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus anchor3 = MakeStatus(3, EnRunMode::Anchor, 3);
        const NodeStatus anchor5 = MakeStatus(5, EnRunMode::Anchor, 5);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        broadcast.PutNode(anchor3, 1000);
        broadcast.PutNode(anchor5, 1000);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        CompleteCurrentTarget(
            synchronizer, transport, masterStatus, anchor3, 0);
        CompleteCurrentTarget(
            synchronizer, transport, masterStatus, anchor5, 0);
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        broadcast.RemoveNode(anchor5);
        fakeTimeUs += 30000000;
        broadcast.PutNode(anchor3, static_cast<uint32_t>(fakeTimeUs / 1000U));
        synchronizer.Update();
        TEST_ASSERT_FALSE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(4, CountRequestsForTarget(transport, 3));
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 5));

        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_FALSE(synchronizer.TryGetNodeSynchronization(
            anchor5.nodeID,
            synchronization));
        CompleteCurrentTarget(
            synchronizer, transport, masterStatus, anchor3, 0);
        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(3, CountRequestsForTarget(transport, 5));
    }

    /**
     * @brief 重複・期限切れを除外し最大16件をノードID昇順で同期することを検証します。
     */
    void TestLateNodeFilteringAndMaximum()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus duplicate2a = MakeStatus(2, EnRunMode::Anchor, 102);
        const NodeStatus duplicate2b = MakeStatus(2, EnRunMode::Tag, 103);
        const NodeStatus expired3 = MakeStatus(3, EnRunMode::Anchor, 3);
        NodeStatus validNodes[17]{};
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        broadcast.PutNode(duplicate2a, 40000);
        broadcast.PutNode(duplicate2b, 40000);
        broadcast.PutNode(expired3, 0);
        for (size_t index = 0; index < 17; ++index)
        {
            validNodes[index] = MakeStatus(
                static_cast<uint8_t>(index + 4U),
                EnRunMode::Anchor,
                static_cast<uint8_t>(index + 4U));
            broadcast.PutNode(validNodes[index], 40000);
        }
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 40000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        for (size_t index = 0;
             index < NtpTimeSynchronizer::m_maxTargetCount;
             ++index)
        {
            NtpSyncRequestPacket request{};
            const auto& wire = transport.GetSentPackets().back().payload;
            TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
                wire.data(), wire.size(), request));
            TEST_ASSERT_EQUAL_UINT8(index + 4U, request.targetNodeId);
            CompleteCurrentTarget(
                synchronizer,
                transport,
                masterStatus,
                validNodes[index],
                0);
        }

        TEST_ASSERT_TRUE(synchronizer.IsSynchronizationComplete());
        TEST_ASSERT_EQUAL_UINT32(
            NtpTimeSynchronizer::m_maxTargetCount *
                (NtpTimeSynchronizer::m_sampleCountPerNode + 1U),
            transport.GetSentPackets().size());
        NodeTimeSynchronization synchronization{};
        TEST_ASSERT_FALSE(synchronizer.TryGetNodeSynchronization(
            2,
            synchronization));
        TEST_ASSERT_FALSE(synchronizer.TryGetNodeSynchronization(
            3,
            synchronization));
        TEST_ASSERT_FALSE(synchronizer.TryGetNodeSynchronization(
            20,
            synchronization));
    }

    /**
     * @brief NodeStatusと将来Range packetをNTP consumerが破棄しないことを検証します。
     */
    void TestForeignPacketsAreNotConsumed()
    {
        const NodeStatus localStatus = MakeStatus(4, EnRunMode::Tag, 4);
        EspNowTransport transport;
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(localStatus);
        TagMasterCoordinator coordinator;
        ConfigRuntime config;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        EspNowReceivedPacket nodeStatusPacket{};
        NtpPacketHeader nodeStatusHeader{
            NtpTimeProtocolCodec::m_magic,
            2,
            1,
            1,
            1,
        };
        nodeStatusPacket.payloadLength = sizeof(nodeStatusHeader);
        memcpy(
            nodeStatusPacket.payload,
            &nodeStatusHeader,
            sizeof(nodeStatusHeader));
        EspNowReceivedPacket rangePacket{};
        NtpPacketHeader rangeHeader{
            NtpTimeProtocolCodec::m_magic,
            1,
            5,
            1,
            1,
        };
        rangePacket.payloadLength = sizeof(rangeHeader);
        memcpy(rangePacket.payload, &rangeHeader, sizeof(rangeHeader));
        transport.PushReceived(nodeStatusPacket);
        transport.PushReceived(rangePacket);

        synchronizer.Update();
        TEST_ASSERT_EQUAL_UINT32(2, transport.GetReceiveCount());
    }

    /**
     * @brief 送信idle待ちとノードID昇順の同期開始を検証します。
     */
    void TestSendIdleAndNodeIdOrder()
    {
        const NodeStatus masterStatus = MakeStatus(1, EnRunMode::Tag, 1);
        const NodeStatus node7 = MakeStatus(7, EnRunMode::Anchor, 7);
        const NodeStatus node3 = MakeStatus(3, EnRunMode::Anchor, 3);
        EspNowTransport transport;
        transport.SetSendIdle(false);
        EspNowBroadcast broadcast;
        broadcast.SetLocalStatus(masterStatus);
        broadcast.PutNode(node7, 1000);
        broadcast.PutNode(node3, 1000);
        TagMasterCoordinator coordinator;
        coordinator.SetMaster(
            MakeMasterIdentity(masterStatus, 77),
            true);
        ConfigRuntime config;
        fakeTimeUs = 1000000;
        NtpTimeSynchronizer synchronizer(
            transport,
            broadcast,
            coordinator,
            config,
            GetFakeTimeUs);

        synchronizer.Update();
        TEST_ASSERT_TRUE(transport.GetSentPackets().empty());
        transport.SetSendIdle(true);
        fakeTimeUs = 1000100;
        synchronizer.Update();
        TEST_ASSERT_EQUAL_UINT32(1, transport.GetSentPackets().size());
        NtpSyncRequestPacket request{};
        const auto& wire = transport.GetSentPackets().back().payload;
        TEST_ASSERT_TRUE(NtpTimeProtocolCodec::DecodeRequest(
            wire.data(),
            wire.size(),
            request));
        TEST_ASSERT_EQUAL_UINT8(3, request.targetNodeId);
        TEST_ASSERT_EQUAL_UINT32(1000100, request.t1);
    }
}

/**
 * @brief 各Unity test前に疑似時刻を初期化します。
 */
void setUp()
{
    fakeTimeUs = 0;
}

/**
 * @brief Unity test後の追加処理はありません。
 */
void tearDown()
{
}

/**
 * @brief native buildが参照するESP timer代替関数です。
 *
 * @return 設定済み疑似時刻
 */
extern "C" int64_t esp_timer_get_time()
{
    return static_cast<int64_t>(fakeTimeUs);
}

/**
 * @brief T-004のPlatformIO native test suiteを実行します。
 *
 * @return Unity test結果
 */
int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestPacketCodecValidation);
    RUN_TEST(TestOffsetAndRoundTripCalculation);
    RUN_TEST(TestTimestampWrapAndExtension);
    RUN_TEST(TestBestSampleSelection);
    RUN_TEST(TestTimeQuality);
    RUN_TEST(TestCurrentMasterTimeForSelfMaster);
    RUN_TEST(TestCurrentMasterTimeForFollower);
    RUN_TEST(TestOneNodeSynchronizationFlowAndMasterReset);
    RUN_TEST(TestFollowerCommitOffsetAndAge);
    RUN_TEST(TestFollowerCommitWrapConversion);
    RUN_TEST(TestFollowerMovingEpochReference);
    RUN_TEST(TestAnchorCommitCompletesSynchronizationAndConversion);
    RUN_TEST(TestLateNodeDiscoveryBeforePeriodicResynchronization);
    RUN_TEST(TestFailedTargetRetriesAfterOneSecond);
    RUN_TEST(TestSuccessfulSynchronizationResynchronizesAfterThirtySeconds);
    RUN_TEST(TestPeriodicResynchronizationExcludesMissingNode);
    RUN_TEST(TestLateNodeFilteringAndMaximum);
    RUN_TEST(TestForeignPacketsAreNotConsumed);
    RUN_TEST(TestSendIdleAndNodeIdOrder);
    return UNITY_END();
}

#endif
