#include <unity.h>

#include <cstdio>
#include <cstring>

#include "EspNowBroadcast.h"
#include "EspNowTransport.h"
#include "NtpTimeSynchronizer.h"
#include "Ryuw122Controller.h"
#include "SequentialRangingController.h"
#include "TagMasterCoordinator.h"

namespace
{
    uint64_t g_nowUs = 1000;

    uint64_t GetNowUs()
    {
        return g_nowUs;
    }

    NodeStatus MakeNode(uint8_t nodeId, EnRunMode mode)
    {
        NodeStatus status{};
        status.nodeID = nodeId;
        status.mode = mode;
        status.macAddress[0] = 0x02;
        status.macAddress[5] = nodeId;
        snprintf(status.uwbAddress, sizeof(status.uwbAddress),
            "%08u", static_cast<unsigned>(nodeId));
        return status;
    }

    TagMasterIdentity MakeMaster(const NodeStatus& status,
        uint32_t sessionId = 100)
    {
        TagMasterIdentity identity{};
        identity.isValid = true;
        identity.nodeID = status.nodeID;
        memcpy(identity.macAddress.data(), status.macAddress, 6);
        identity.sessionId = sessionId;
        return identity;
    }

    void ConfigureBroadcast(EspNowBroadcast& broadcast,
        const NodeStatus& local, const NodeStatus* nodes, size_t nodeCount)
    {
        broadcast.SetLocal(local);
        for (size_t index = 0; index < nodeCount; ++index)
        {
            if (nodes[index].nodeID != local.nodeID)
            {
                broadcast.AddNode(nodes[index]);
            }
        }
    }

    EspNowReceivedPacket MakeReceived(const StubSentPacket& sent,
        const uint8_t sourceMac[6], uint32_t receivedUs = 100)
    {
        EspNowReceivedPacket received{};
        memcpy(received.sourceMac, sourceMac, 6);
        memcpy(received.destinationMac, sent.destinationMac, 6);
        received.rssi = -40;
        received.receivedTimestampUs = receivedUs;
        received.payloadLength = sent.payloadLength;
        memcpy(received.payload, sent.payload, sent.payloadLength);
        return received;
    }

    RangingNodeIdentity MakeIdentity(const NodeStatus& status)
    {
        RangingNodeIdentity identity{};
        identity.nodeId = status.nodeID;
        memcpy(identity.macAddress, status.macAddress, 6);
        memcpy(identity.uwbAddress, status.uwbAddress, 9);
        return identity;
    }

    RangeMeasurementData MakeMeasurement(const NodeStatus& master,
        const NodeStatus* anchors, uint8_t anchorCount,
        const NodeStatus* tags, uint8_t tagCount,
        uint8_t anchorIndex, uint8_t tagIndex)
    {
        RangeMeasurementData measurement{};
        measurement.roundId = 1;
        measurement.pairSequence = static_cast<uint16_t>(
            anchorIndex * tagCount + tagIndex + 1U);
        measurement.masterTagId = master.nodeID;
        memcpy(measurement.masterMac, master.macAddress, 6);
        measurement.anchorCount = anchorCount;
        measurement.tagCount = tagCount;
        measurement.anchorIndex = anchorIndex;
        measurement.tagIndex = tagIndex;
        measurement.anchor = MakeIdentity(anchors[anchorIndex]);
        measurement.tag = MakeIdentity(tags[tagIndex]);
        measurement.status = EnRangeResultStatus::Success;
        measurement.distanceMm = 1234;
        measurement.uwbRssi = -70;
        measurement.commandReceivedUs = 100;
        measurement.rangingStartedUs = 110;
        measurement.rangingCompletedUs = 130;
        measurement.isLastMeasurement =
            anchorIndex == anchorCount - 1U && tagIndex == tagCount - 1U;
        return measurement;
    }

    EspNowReceivedPacket EncodeMeasurement(
        SequentialRangingProtocolCodec& codec,
        uint32_t sessionId,
        uint32_t sequence,
        const RangeMeasurementData& measurement,
        const uint8_t destinationMac[6],
        const uint8_t sourceMac[6])
    {
        RangeMeasurementPacket wire{};
        TEST_ASSERT_TRUE(codec.EncodeMeasurement(
            sessionId, sequence, measurement, wire));
        EspNowReceivedPacket packet{};
        memcpy(packet.sourceMac, sourceMac, 6);
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.rssi = -41;
        packet.payloadLength = sizeof(wire);
        memcpy(packet.payload, &wire, sizeof(wire));
        return packet;
    }

    EspNowReceivedPacket EncodeControl(
        SequentialRangingProtocolCodec& codec,
        uint32_t sessionId,
        uint32_t sequence,
        const RangeControlData& control,
        const uint8_t destinationMac[6],
        const uint8_t sourceMac[6],
        uint32_t receivedUs)
    {
        RangeControlPacket wire{};
        TEST_ASSERT_TRUE(codec.EncodeControl(
            sessionId, sequence, control, wire));
        EspNowReceivedPacket packet{};
        memcpy(packet.sourceMac, sourceMac, 6);
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.receivedTimestampUs = receivedUs;
        packet.payloadLength = sizeof(wire);
        memcpy(packet.payload, &wire, sizeof(wire));
        return packet;
    }

    EspNowReceivedPacket EncodeForward(
        SequentialRangingProtocolCodec& codec,
        uint32_t sessionId,
        uint32_t sequence,
        RangeMeasurementData measurement,
        const uint8_t destinationMac[6],
        const uint8_t sourceMac[6])
    {
        measurement.commandReceivedMasterTimeUs =
            measurement.commandReceivedUs;
        measurement.rangingStartedMasterTimeUs =
            measurement.rangingStartedUs;
        measurement.rangingCompletedMasterTimeUs =
            measurement.rangingCompletedUs;
        measurement.synchronizationRoundTripUs = 20;
        measurement.synchronizationAgeUs = 30;
        measurement.timeQuality = EnTimeQuality::Synchronized;
        RangeMeasurementPacket wire{};
        TEST_ASSERT_TRUE(codec.EncodeMeasurementForward(
            sessionId, sequence, measurement, wire));
        EspNowReceivedPacket packet{};
        memcpy(packet.sourceMac, sourceMac, 6);
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.payloadLength = sizeof(wire);
        memcpy(packet.payload, &wire, sizeof(wire));
        return packet;
    }

    EspNowReceivedPacket EncodeComplete(
        SequentialRangingProtocolCodec& codec,
        uint32_t sessionId,
        uint32_t sequence,
        const RangeRoundCompleteData& complete,
        const uint8_t destinationMac[6],
        const uint8_t sourceMac[6])
    {
        RangeRoundCompletePacket wire{};
        TEST_ASSERT_TRUE(codec.EncodeRoundComplete(
            sessionId, sequence, complete, wire));
        EspNowReceivedPacket packet{};
        memcpy(packet.sourceMac, sourceMac, 6);
        memcpy(packet.destinationMac, destinationMac, 6);
        packet.payloadLength = sizeof(wire);
        memcpy(packet.payload, &wire, sizeof(wire));
        return packet;
    }

    StubSentPacket TransferSent(
        EspNowTransport& sourceTransport,
        const NodeStatus& source,
        EspNowTransport& destinationTransport)
    {
        StubSentPacket sent{};
        TEST_ASSERT_TRUE(sourceTransport.TakeSent(sent));
        destinationTransport.Inject(MakeReceived(
            sent, source.macAddress, static_cast<uint32_t>(g_nowUs)));
        return sent;
    }

    void CompleteSuccess(
        Ryuw122Controller& ryuw122,
        const NodeStatus& tag,
        uint32_t distanceMm)
    {
        Ryuw122RangingResult result{};
        memcpy(result.tagAddress, tag.uwbAddress, 9);
        result.status = EnRyuw122RangingStatus::Success;
        result.reason = EnRyuw122RangingReason::Success;
        result.distanceMm = distanceMm;
        result.uwbRssi = -60;
        result.startedAtUs = static_cast<uint32_t>(g_nowUs + 1U);
        result.completedAtUs = static_cast<uint32_t>(g_nowUs + 2U);
        g_nowUs += 2U;
        ryuw122.Complete(result);
    }
}

void setUp()
{
    g_nowUs = 1000;
}

void tearDown()
{
}

void TestFollowerDoesNotStartRound()
{
    const NodeStatus nodes[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(2, EnRunMode::Tag),
        MakeNode(10, EnRunMode::Anchor)};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, nodes[1], nodes, 3);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(nodes[0]), false);
    NtpTimeSynchronizer synchronizer;
    synchronizer.SetSynchronized(2);
    Ryuw122Controller ryuw122;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        synchronizer, ryuw122, codec, GetNowUs);
    controller.Begin();
    controller.Update();

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(EnSequentialRangingState::FollowingMaster),
        static_cast<uint8_t>(controller.GetState()));
    TEST_ASSERT_EQUAL_UINT32(0, ryuw122.StartCount());
    StubSentPacket sent{};
    TEST_ASSERT_FALSE(transport.TakeSent(sent));
}

void TestFollowerRejectsReorderedAndDuplicateRoundComplete()
{
    const NodeStatus tags[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(2, EnRunMode::Tag)};
    const NodeStatus anchor = MakeNode(10, EnRunMode::Anchor);
    const NodeStatus all[] = {tags[0], tags[1], anchor};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tags[1], all, 3);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tags[0]), false);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(2);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();

    RangeMeasurementData measurement = MakeMeasurement(tags[0], &anchor, 1,
        tags, 2, 0, 1);
    measurement.roundId = 2;
    transport.Inject(EncodeForward(codec, 100, 10, measurement,
        tags[1].macAddress, tags[0].macAddress));
    controller.Update();
    TimedRangeMeasurement event{};
    TEST_ASSERT_TRUE(controller.TryTakeMeasurement(event));
    TEST_ASSERT_EQUAL_UINT32(2, event.roundId);

    RangeRoundCompleteData stale{};
    stale.roundId = 1;
    stale.nextRoundId = 2;
    stale.masterTagId = 1;
    memcpy(stale.masterMac, tags[0].macAddress, 6);
    stale.startedMasterTimeUs = 100;
    stale.completedMasterTimeUs = 200;
    stale.anchorCount = 1;
    stale.tagCount = 2;
    stale.expectedMeasurementCount = 2;
    stale.receivedMeasurementCount = 2;
    transport.Inject(EncodeComplete(codec, 100, 20, stale,
        tags[1].macAddress, tags[0].macAddress));
    controller.Update();
    SequentialRangeRoundSummary summary{};
    TEST_ASSERT_FALSE(controller.TryTakeCompletedRound(summary));

    RangeRoundCompleteData current = stale;
    current.roundId = 2;
    current.nextRoundId = 3;
    current.startedMasterTimeUs = 300;
    current.completedMasterTimeUs = 400;
    transport.Inject(EncodeComplete(codec, 100, 21, current,
        tags[1].macAddress, tags[0].macAddress));
    controller.Update();
    TEST_ASSERT_TRUE(controller.TryTakeCompletedRound(summary));
    TEST_ASSERT_EQUAL_UINT32(2, summary.roundId);

    transport.Inject(EncodeComplete(codec, 100, 22, current,
        tags[1].macAddress, tags[0].macAddress));
    controller.Update();
    TEST_ASSERT_FALSE(controller.TryTakeCompletedRound(summary));
}

void TestOneAnchorOneTagRunsTwoRoundsContinuously()
{
    const NodeStatus nodes[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(10, EnRunMode::Anchor)};
    EspNowTransport masterTransport;
    EspNowBroadcast masterBroadcast;
    ConfigureBroadcast(masterBroadcast, nodes[0], nodes, 2);
    TagMasterCoordinator masterCoordinator;
    masterCoordinator.SetMaster(MakeMaster(nodes[0]), true);
    NtpTimeSynchronizer masterSync;
    masterSync.SetSynchronized(10);
    Ryuw122Controller masterRyuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController master(masterTransport, masterBroadcast,
        masterCoordinator, masterSync, masterRyuw, codec, GetNowUs);

    EspNowTransport anchorTransport;
    EspNowBroadcast anchorBroadcast;
    ConfigureBroadcast(anchorBroadcast, nodes[1], nodes, 2);
    TagMasterCoordinator anchorCoordinator;
    anchorCoordinator.SetMaster(MakeMaster(nodes[0]), false);
    NtpTimeSynchronizer anchorSync;
    anchorSync.SetSynchronized(10);
    Ryuw122Controller anchorRyuw;
    SequentialRangingController anchor(anchorTransport, anchorBroadcast,
        anchorCoordinator, anchorSync, anchorRyuw, codec, GetNowUs);

    master.Begin();
    anchor.Begin();
    master.Update();
    StubSentPacket control{};
    TEST_ASSERT_TRUE(masterTransport.TakeSent(control));
    anchorTransport.Inject(MakeReceived(control, nodes[0].macAddress));
    anchor.Update();
    TEST_ASSERT_EQUAL_UINT32(1, anchorRyuw.StartCount());

    Ryuw122RangingResult result{};
    memcpy(result.tagAddress, nodes[0].uwbAddress, 9);
    result.status = EnRyuw122RangingStatus::Success;
    result.reason = EnRyuw122RangingReason::Success;
    result.distanceMm = 1000;
    result.uwbRssi = -60;
    result.startedAtUs = 110;
    result.completedAtUs = 130;
    anchorRyuw.Complete(result);
    anchor.Update();
    RangingDiagnosticEvent diagnostic{};
    TEST_ASSERT_TRUE(anchor.TryTakeDiagnostic(diagnostic));
    TEST_ASSERT_EQUAL_UINT8(10U, diagnostic.anchorId);
    TEST_ASSERT_EQUAL_UINT8(1U, diagnostic.tagId);
    TEST_ASSERT_EQUAL_STRING(nodes[0].uwbAddress, diagnostic.tagAddress);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(EnRyuw122RangingReason::Success),
        static_cast<uint8_t>(diagnostic.reason));
    TEST_ASSERT_EQUAL_UINT32(1000U, diagnostic.distanceMm);
    StubSentPacket measurement{};
    TEST_ASSERT_TRUE(anchorTransport.TakeSent(measurement));
    masterTransport.Inject(MakeReceived(measurement, nodes[1].macAddress));
    master.Update();

    TimedRangeMeasurement event{};
    TEST_ASSERT_TRUE(master.TryTakeMeasurement(event));
    TEST_ASSERT_EQUAL_UINT32(1, event.roundId);
    SequentialRangeRoundSummary summary{};
    TEST_ASSERT_TRUE(master.TryTakeCompletedRound(summary));
    TEST_ASSERT_FALSE(summary.timedOut);
    StubSentPacket nextControl{};
    TEST_ASSERT_TRUE(masterTransport.TakeSent(nextControl));
    uint32_t session = 0;
    uint32_t sequence = 0;
    RangeControlData decoded{};
    TEST_ASSERT_TRUE(codec.DecodeControl(nextControl.payload,
        nextControl.payloadLength, session, sequence, decoded));
    TEST_ASSERT_EQUAL_UINT32(2, decoded.roundId);
}

void TestRoundCompletionWaitsForNewSynchronization()
{
    const NodeStatus tag = MakeNode(1, EnRunMode::Tag);
    const NodeStatus anchor = MakeNode(10, EnRunMode::Anchor);
    const NodeStatus all[] = {tag, anchor};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tag, all, 2);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tag), true);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    controller.Update();
    StubSentPacket initialControl{};
    TEST_ASSERT_TRUE(transport.TakeSent(initialControl));

    sync.m_complete = false;
    RangeMeasurementData measurement = MakeMeasurement(tag, &anchor, 1,
        &tag, 1, 0, 0);
    transport.Inject(EncodeMeasurement(codec, 100, 10, measurement,
        tag.macAddress, anchor.macAddress));
    controller.Update();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            EnSequentialRangingState::WaitingForSynchronization),
        static_cast<uint8_t>(controller.GetState()));
    StubSentPacket blockedControl{};
    TEST_ASSERT_FALSE(transport.TakeSent(blockedControl));

    sync.m_complete = true;
    controller.Update();
    StubSentPacket nextControl{};
    TEST_ASSERT_TRUE(transport.TakeSent(nextControl));
    uint32_t session = 0;
    uint32_t sequence = 0;
    RangeControlData decoded{};
    TEST_ASSERT_TRUE(codec.DecodeControl(nextControl.payload,
        nextControl.payloadLength, session, sequence, decoded));
    TEST_ASSERT_EQUAL_UINT32(2, decoded.roundId);
}

/**
 * @brief timeout drain中の次roundを保留しSTART失敗と重複送信を生成しないことを確認します。
 */
void TestBusyDrainDefersNextRoundWithoutStartFailure()
{
    const NodeStatus nodes[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(10, EnRunMode::Anchor)};
    EspNowTransport masterTransport;
    EspNowBroadcast masterBroadcast;
    ConfigureBroadcast(masterBroadcast, nodes[0], nodes, 2);
    TagMasterCoordinator masterCoordinator;
    masterCoordinator.SetMaster(MakeMaster(nodes[0]), true);
    NtpTimeSynchronizer masterSync;
    masterSync.SetSynchronized(10);
    Ryuw122Controller masterRyuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController master(masterTransport, masterBroadcast,
        masterCoordinator, masterSync, masterRyuw, codec, GetNowUs);

    EspNowTransport anchorTransport;
    EspNowBroadcast anchorBroadcast;
    ConfigureBroadcast(anchorBroadcast, nodes[1], nodes, 2);
    TagMasterCoordinator anchorCoordinator;
    anchorCoordinator.SetMaster(MakeMaster(nodes[0]), false);
    NtpTimeSynchronizer anchorSync;
    anchorSync.SetSynchronized(10);
    Ryuw122Controller anchorRyuw;
    SequentialRangingController anchor(anchorTransport, anchorBroadcast,
        anchorCoordinator, anchorSync, anchorRyuw, codec, GetNowUs);

    master.Begin();
    anchor.Begin();
    master.Update();
    TransferSent(masterTransport, nodes[0], anchorTransport);
    anchor.Update();
    TEST_ASSERT_EQUAL_UINT32(1U, anchorRyuw.StartCount());

    Ryuw122RangingResult timeout{};
    memcpy(timeout.tagAddress, nodes[0].uwbAddress, 9);
    timeout.status = EnRyuw122RangingStatus::TimedOut;
    timeout.reason = EnRyuw122RangingReason::Timeout;
    timeout.startedAtUs = 100U;
    timeout.completedAtUs = 300100U;
    anchorRyuw.SetBusy(true);
    anchorRyuw.Complete(timeout);
    anchor.Update();
    RangingDiagnosticEvent diagnostic{};
    TEST_ASSERT_TRUE(anchor.TryTakeDiagnostic(diagnostic));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(EnRyuw122RangingReason::Timeout),
        static_cast<uint8_t>(diagnostic.reason));
    TEST_ASSERT_FALSE(anchor.TryTakeDiagnostic(diagnostic));

    TransferSent(anchorTransport, nodes[1], masterTransport);
    master.Update();
    TransferSent(masterTransport, nodes[0], anchorTransport);
    anchor.Update();
    TEST_ASSERT_EQUAL_UINT32(1U, anchorRyuw.StartCount());
    TEST_ASSERT_FALSE(anchor.TryTakeDiagnostic(diagnostic));
    StubSentPacket prematureMeasurement{};
    TEST_ASSERT_FALSE(anchorTransport.TakeSent(prematureMeasurement));

    anchor.Update();
    TEST_ASSERT_EQUAL_UINT32(1U, anchorRyuw.StartCount());
    anchorRyuw.SetBusy(false);
    anchor.Update();
    TEST_ASSERT_EQUAL_UINT32(2U, anchorRyuw.StartCount());
    anchor.Update();
    TEST_ASSERT_EQUAL_UINT32(2U, anchorRyuw.StartCount());
}

/**
 * @brief session切替cycleの旧結果を破棄して新しい測距を開始できることを確認します。
 */
void TestSessionChangeDiscardsStaleResultBeforeNewControlStarts()
{
    const NodeStatus nodes[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(10, EnRunMode::Anchor)};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, nodes[1], nodes, 2);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(nodes[0], 100U), false);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();

    RangeControlData control{};
    control.roundId = 1U;
    control.pairSequence = 1U;
    control.masterTagId = nodes[0].nodeID;
    memcpy(control.masterMac, nodes[0].macAddress, 6);
    control.anchorCount = 1U;
    control.tagCount = 1U;
    control.anchorIndex = 0U;
    control.anchorIds[0] = nodes[1].nodeID;
    control.tagIds[0] = nodes[0].nodeID;
    transport.Inject(EncodeControl(codec, 100U, 1U, control,
        nodes[1].macAddress, nodes[0].macAddress,
        static_cast<uint32_t>(g_nowUs)));
    controller.Update();
    TEST_ASSERT_EQUAL_UINT32(1U, ryuw.StartCount());

    CompleteSuccess(ryuw, nodes[0], 1000U);
    coordinator.SetMaster(MakeMaster(nodes[0], 200U), false);
    transport.Inject(EncodeControl(codec, 200U, 1U, control,
        nodes[1].macAddress, nodes[0].macAddress,
        static_cast<uint32_t>(g_nowUs)));
    controller.Update();
    TEST_ASSERT_EQUAL_UINT32(2U, ryuw.StartCount());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(EnSequentialRangingState::AnchorRanging),
        static_cast<uint8_t>(controller.GetState()));
    RangingDiagnosticEvent diagnostic{};
    TEST_ASSERT_FALSE(controller.TryTakeDiagnostic(diagnostic));

    CompleteSuccess(ryuw, nodes[0], 2000U);
    controller.Update();
    TEST_ASSERT_TRUE(controller.TryTakeDiagnostic(diagnostic));
    TEST_ASSERT_EQUAL_UINT32(2000U, diagnostic.distanceMm);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(EnSequentialRangingState::AnchorIdle),
        static_cast<uint8_t>(controller.GetState()));
}

void TestNewRoundAcceptsControlFromChangedSourceWithLowerSequence()
{
    const NodeStatus nodes[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(10, EnRunMode::Anchor),
        MakeNode(20, EnRunMode::Anchor)};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, nodes[2], nodes, 3);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(nodes[0]), false);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(20);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();

    RangeControlData first{};
    first.roundId = 1;
    first.pairSequence = 2;
    first.masterTagId = 1;
    memcpy(first.masterMac, nodes[0].macAddress, 6);
    first.anchorCount = 2;
    first.tagCount = 1;
    first.anchorIndex = 1;
    first.anchorIds[0] = 10;
    first.anchorIds[1] = 20;
    first.tagIds[0] = 1;
    transport.Inject(EncodeControl(codec, 100, 100, first,
        nodes[2].macAddress, nodes[1].macAddress,
        static_cast<uint32_t>(g_nowUs)));
    controller.Update();
    TEST_ASSERT_EQUAL_UINT32(1, ryuw.StartCount());
    CompleteSuccess(ryuw, nodes[0], 1000);
    controller.Update();

    RangeControlData second{};
    second.roundId = 2;
    second.pairSequence = 1;
    second.masterTagId = 1;
    memcpy(second.masterMac, nodes[0].macAddress, 6);
    second.anchorCount = 1;
    second.tagCount = 1;
    second.anchorIndex = 0;
    second.anchorIds[0] = 20;
    second.tagIds[0] = 1;
    transport.Inject(EncodeControl(codec, 100, 5, second,
        nodes[2].macAddress, nodes[0].macAddress,
        static_cast<uint32_t>(g_nowUs)));
    controller.Update();
    TEST_ASSERT_EQUAL_UINT32(2, ryuw.StartCount());
}

void TestTwoAnchorTwoTagOrderAndAnchorForwarding()
{
    const NodeStatus nodes[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(2, EnRunMode::Tag),
        MakeNode(10, EnRunMode::Anchor), MakeNode(20, EnRunMode::Anchor)};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, nodes[2], nodes, 4);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(nodes[0]), false);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();

    RangeControlData control{};
    control.roundId = 1;
    control.pairSequence = 1;
    control.masterTagId = 1;
    memcpy(control.masterMac, nodes[0].macAddress, 6);
    control.anchorCount = 2;
    control.tagCount = 2;
    control.anchorIds[0] = 10;
    control.anchorIds[1] = 20;
    control.tagIds[0] = 1;
    control.tagIds[1] = 2;
    RangeControlPacket wire{};
    TEST_ASSERT_TRUE(codec.EncodeControl(100, 1, control, wire));
    EspNowReceivedPacket received{};
    memcpy(received.sourceMac, nodes[0].macAddress, 6);
    memcpy(received.destinationMac, nodes[2].macAddress, 6);
    received.receivedTimestampUs = 100;
    received.payloadLength = sizeof(wire);
    memcpy(received.payload, &wire, sizeof(wire));
    transport.Inject(received);
    controller.Update();
    TEST_ASSERT_EQUAL_STRING(nodes[0].uwbAddress, ryuw.StartedAt(0));

    Ryuw122RangingResult result{};
    result.status = EnRyuw122RangingStatus::Success;
    result.distanceMm = 1000;
    result.uwbRssi = -60;
    result.startedAtUs = 110;
    result.completedAtUs = 130;
    ryuw.Complete(result);
    controller.Update();
    TEST_ASSERT_EQUAL_STRING(nodes[1].uwbAddress, ryuw.StartedAt(1));
    StubSentPacket firstMeasurement{};
    TEST_ASSERT_TRUE(transport.TakeSent(firstMeasurement));

    result.status = EnRyuw122RangingStatus::TimedOut;
    result.distanceMm = 0;
    result.uwbRssi = 0;
    result.startedAtUs = 1010;
    result.completedAtUs = 1030;
    ryuw.Complete(result);
    controller.Update();
    StubSentPacket forwardedControl{};
    TEST_ASSERT_TRUE(transport.TakeSent(forwardedControl));
    uint32_t session = 0;
    uint32_t sequence = 0;
    RangeControlData decoded{};
    TEST_ASSERT_TRUE(codec.DecodeControl(forwardedControl.payload,
        forwardedControl.payloadLength, session, sequence, decoded));
    TEST_ASSERT_EQUAL_UINT8(1, decoded.anchorIndex);
    TEST_ASSERT_EQUAL_UINT16(3, decoded.pairSequence);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(nodes[3].macAddress,
        forwardedControl.destinationMac, 6);
    controller.Update();
    StubSentPacket failedMeasurement{};
    TEST_ASSERT_TRUE(transport.TakeSent(failedMeasurement));
    RangeMeasurementData decodedMeasurement{};
    TEST_ASSERT_TRUE(codec.DecodeMeasurement(failedMeasurement.payload,
        failedMeasurement.payloadLength,
        EnSequentialRangingPacketType::RangeMeasurement,
        session, sequence, decodedMeasurement));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(EnRangeResultStatus::TimedOut),
        static_cast<uint8_t>(decodedMeasurement.status));
}

void TestMasterPublishesEachTwoByTwoMeasurementInOrder()
{
    const NodeStatus tags[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(2, EnRunMode::Tag)};
    const NodeStatus anchors[] = {
        MakeNode(10, EnRunMode::Anchor), MakeNode(20, EnRunMode::Anchor)};
    const NodeStatus all[] = {tags[0], tags[1], anchors[0], anchors[1]};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tags[0], all, 4);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tags[0]), true);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(2);
    sync.SetSynchronized(10);
    sync.SetSynchronized(20);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    controller.Update();
    StubSentPacket initialControl{};
    TEST_ASSERT_TRUE(transport.TakeSent(initialControl));

    const uint8_t expectedAnchor[] = {10, 10, 20, 20};
    const uint8_t expectedTag[] = {1, 2, 1, 2};
    for (uint8_t index = 0; index < 4; ++index)
    {
        const uint8_t anchorIndex = index / 2U;
        const uint8_t tagIndex = index % 2U;
        RangeMeasurementData measurement = MakeMeasurement(tags[0], anchors,
            2, tags, 2, anchorIndex, tagIndex);
        transport.Inject(EncodeMeasurement(codec, 100, index + 10U,
            measurement, tags[0].macAddress,
            anchors[anchorIndex].macAddress));
        controller.Update();
        TimedRangeMeasurement event{};
        TEST_ASSERT_TRUE(controller.TryTakeMeasurement(event));
        TEST_ASSERT_EQUAL_UINT8(expectedAnchor[index], event.anchorId);
        TEST_ASSERT_EQUAL_UINT8(expectedTag[index], event.tagId);
        if (index == 1U)
        {
            StubSentPacket forward{};
            TEST_ASSERT_TRUE(transport.TakeSent(forward));
            uint32_t session = 0;
            uint32_t sequence = 0;
            RangeMeasurementData forwarded{};
            TEST_ASSERT_TRUE(codec.DecodeMeasurement(forward.payload,
                forward.payloadLength,
                EnSequentialRangingPacketType::RangeMeasurementForward,
                session, sequence, forwarded));
            TEST_ASSERT_EQUAL_UINT8_ARRAY(tags[1].macAddress,
                forward.destinationMac, 6);
            TEST_ASSERT_EQUAL_UINT8(2, forwarded.tag.nodeId);
        }
        if (index < 3U)
        {
            SequentialRangeRoundSummary summary{};
            TEST_ASSERT_FALSE(controller.TryTakeCompletedRound(summary));
        }
    }
    SequentialRangeRoundSummary summary{};
    TEST_ASSERT_TRUE(controller.TryTakeCompletedRound(summary));
    TEST_ASSERT_EQUAL_UINT8(4, summary.receivedMeasurementCount);
}

void TestDuplicateWrongSourceAndWrongSessionAreIgnored()
{
    const NodeStatus tag = MakeNode(1, EnRunMode::Tag);
    const NodeStatus anchor = MakeNode(10, EnRunMode::Anchor);
    const NodeStatus all[] = {tag, anchor};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tag, all, 2);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tag), true);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    controller.Update();
    RangeMeasurementData measurement = MakeMeasurement(tag, &anchor, 1,
        &tag, 1, 0, 0);
    EspNowReceivedPacket valid = EncodeMeasurement(codec, 100, 10,
        measurement, tag.macAddress, anchor.macAddress);
    EspNowReceivedPacket wrongSource = valid;
    wrongSource.sourceMac[5] = 99;
    EspNowReceivedPacket wrongSession = EncodeMeasurement(codec, 101, 11,
        measurement, tag.macAddress, anchor.macAddress);
    transport.Inject(wrongSource);
    transport.Inject(wrongSession);
    transport.Inject(valid);
    transport.Inject(valid);
    controller.Update();
    TimedRangeMeasurement event{};
    TEST_ASSERT_TRUE(controller.TryTakeMeasurement(event));
    TEST_ASSERT_FALSE(controller.TryTakeMeasurement(event));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2,
        controller.GetDiagnostics().invalidPacketCount);
}

void TestForeignPacketIsNotConsumed()
{
    const NodeStatus tag = MakeNode(1, EnRunMode::Tag);
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    broadcast.SetLocal(tag);
    TagMasterCoordinator coordinator;
    NtpTimeSynchronizer sync;
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    EspNowReceivedPacket foreign{};
    foreign.payloadLength = 4;
    foreign.payload[0] = 0x59;
    foreign.payload[1] = 0x52;
    foreign.payload[2] = 2;
    foreign.payload[3] = 1;
    transport.Inject(foreign);
    controller.Update();
    TEST_ASSERT_EQUAL_UINT32(1, transport.ReceivedCount());
}

void TestMasterChangeClearsQueuedEventsAndRound()
{
    const NodeStatus tag = MakeNode(1, EnRunMode::Tag);
    const NodeStatus anchor = MakeNode(10, EnRunMode::Anchor);
    const NodeStatus all[] = {tag, anchor};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tag, all, 2);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tag), true);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    const uint32_t initialResetGeneration = controller.GetResetGeneration();
    controller.Update();
    RangeMeasurementData measurement = MakeMeasurement(tag, &anchor, 1,
        &tag, 1, 0, 0);
    transport.Inject(EncodeMeasurement(codec, 100, 10, measurement,
        tag.macAddress, anchor.macAddress));
    controller.Update();
    coordinator.SetMaster(MakeMaster(tag, 200), true);
    controller.Update();
    TEST_ASSERT_EQUAL_UINT32(
        initialResetGeneration + 1U,
        controller.GetResetGeneration());
    TimedRangeMeasurement event{};
    SequentialRangeRoundSummary summary{};
    TEST_ASSERT_FALSE(controller.TryTakeMeasurement(event));
    TEST_ASSERT_FALSE(controller.TryTakeCompletedRound(summary));
}

void TestNtpConversionFailurePublishesUnsynchronizedQuality()
{
    const NodeStatus tag = MakeNode(1, EnRunMode::Tag);
    const NodeStatus anchor = MakeNode(10, EnRunMode::Anchor);
    const NodeStatus all[] = {tag, anchor};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tag, all, 2);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tag), true);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    sync.m_conversionFails[10] = true;
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    controller.Update();
    RangeMeasurementData measurement = MakeMeasurement(tag, &anchor, 1,
        &tag, 1, 0, 0);
    transport.Inject(EncodeMeasurement(codec, 100, 10, measurement,
        tag.macAddress, anchor.macAddress));
    controller.Update();
    TimedRangeMeasurement event{};
    TEST_ASSERT_TRUE(controller.TryTakeMeasurement(event));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EnTimeQuality::Unsynchronized),
        static_cast<uint8_t>(event.timeQuality));
    TEST_ASSERT_EQUAL_UINT64(0, event.rangingCompletedMasterTimeUs);
}

void TestRoundTimeoutPublishesFailureSummary()
{
    const NodeStatus tag = MakeNode(1, EnRunMode::Tag);
    const NodeStatus anchor = MakeNode(10, EnRunMode::Anchor);
    const NodeStatus all[] = {tag, anchor};
    EspNowTransport transport;
    EspNowBroadcast broadcast;
    ConfigureBroadcast(broadcast, tag, all, 2);
    TagMasterCoordinator coordinator;
    coordinator.SetMaster(MakeMaster(tag), true);
    NtpTimeSynchronizer sync;
    sync.SetSynchronized(10);
    Ryuw122Controller ryuw;
    SequentialRangingProtocolCodec codec;
    SequentialRangingController controller(transport, broadcast, coordinator,
        sync, ryuw, codec, GetNowUs);
    controller.Begin();
    controller.Update();
    g_nowUs = 500000;
    controller.Update();
    SequentialRangeRoundSummary summary{};
    TEST_ASSERT_TRUE(controller.TryTakeCompletedRound(summary));
    TEST_ASSERT_TRUE(summary.timedOut);
    TEST_ASSERT_EQUAL_UINT8(0, summary.receivedMeasurementCount);
}

void TestThreeAnchorTwoTagConnectedEndToEndFlow()
{
    const NodeStatus tags[] = {
        MakeNode(1, EnRunMode::Tag), MakeNode(2, EnRunMode::Tag)};
    const NodeStatus anchors[] = {
        MakeNode(10, EnRunMode::Anchor), MakeNode(20, EnRunMode::Anchor),
        MakeNode(30, EnRunMode::Anchor)};
    const NodeStatus all[] = {
        tags[0], tags[1], anchors[0], anchors[1], anchors[2]};
    SequentialRangingProtocolCodec codec;

    EspNowTransport masterTransport;
    EspNowBroadcast masterBroadcast;
    ConfigureBroadcast(masterBroadcast, tags[0], all, 5);
    TagMasterCoordinator masterCoordinator;
    masterCoordinator.SetMaster(MakeMaster(tags[0]), true);
    NtpTimeSynchronizer masterSync;
    masterSync.SetSynchronized(2);
    masterSync.SetSynchronized(10);
    masterSync.SetSynchronized(20);
    masterSync.SetSynchronized(30);
    Ryuw122Controller masterRyuw;
    SequentialRangingController master(masterTransport, masterBroadcast,
        masterCoordinator, masterSync, masterRyuw, codec, GetNowUs);

    EspNowTransport followerTransport;
    EspNowBroadcast followerBroadcast;
    ConfigureBroadcast(followerBroadcast, tags[1], all, 5);
    TagMasterCoordinator followerCoordinator;
    followerCoordinator.SetMaster(MakeMaster(tags[0]), false);
    NtpTimeSynchronizer followerSync;
    followerSync.SetSynchronized(2);
    Ryuw122Controller followerRyuw;
    SequentialRangingController follower(followerTransport, followerBroadcast,
        followerCoordinator, followerSync, followerRyuw, codec, GetNowUs);

    EspNowTransport anchorTransports[3];
    EspNowBroadcast anchorBroadcasts[3];
    TagMasterCoordinator anchorCoordinators[3];
    NtpTimeSynchronizer anchorSynchronizers[3];
    Ryuw122Controller anchorRyuws[3];
    for (uint8_t index = 0; index < 3; ++index)
    {
        ConfigureBroadcast(anchorBroadcasts[index], anchors[index], all, 5);
        anchorCoordinators[index].SetMaster(MakeMaster(tags[0]), false);
        anchorSynchronizers[index].SetSynchronized(anchors[index].nodeID);
    }
    SequentialRangingController anchorOne(anchorTransports[0],
        anchorBroadcasts[0], anchorCoordinators[0], anchorSynchronizers[0],
        anchorRyuws[0], codec, GetNowUs);
    SequentialRangingController anchorTwo(anchorTransports[1],
        anchorBroadcasts[1], anchorCoordinators[1], anchorSynchronizers[1],
        anchorRyuws[1], codec, GetNowUs);
    SequentialRangingController anchorThree(anchorTransports[2],
        anchorBroadcasts[2], anchorCoordinators[2], anchorSynchronizers[2],
        anchorRyuws[2], codec, GetNowUs);
    SequentialRangingController* anchorControllers[] = {
        &anchorOne, &anchorTwo, &anchorThree};

    master.Begin();
    follower.Begin();
    for (SequentialRangingController* controller : anchorControllers)
    {
        controller->Begin();
    }
    master.Update();
    StubSentPacket initialControl = TransferSent(masterTransport, tags[0],
        anchorTransports[0]);
    uint32_t session = 0;
    uint32_t sequence = 0;
    RangeControlData decodedControl{};
    TEST_ASSERT_TRUE(codec.DecodeControl(initialControl.payload,
        initialControl.payloadLength, session, sequence, decodedControl));
    TEST_ASSERT_EQUAL_UINT32(1, decodedControl.roundId);
    anchorControllers[0]->Update();

    const uint8_t expectedAnchorIds[] = {10, 10, 20, 20, 30, 30};
    const uint8_t expectedTagIds[] = {1, 2, 1, 2, 1, 2};
    uint8_t masterEventIndex = 0;
    uint8_t followerEventCount = 0;
    StubSentPacket nextRoundControl{};
    for (uint8_t anchorIndex = 0; anchorIndex < 3; ++anchorIndex)
    {
        for (uint8_t tagIndex = 0; tagIndex < 2; ++tagIndex)
        {
            TEST_ASSERT_EQUAL_UINT32(tagIndex + 1U,
                anchorRyuws[anchorIndex].StartCount());
            TEST_ASSERT_EQUAL_STRING(tags[tagIndex].uwbAddress,
                anchorRyuws[anchorIndex].StartedAt(tagIndex));
            CompleteSuccess(anchorRyuws[anchorIndex], tags[tagIndex],
                static_cast<uint32_t>(1000U + masterEventIndex));
            anchorControllers[anchorIndex]->Update();

            if (tagIndex == 1U && anchorIndex < 2U)
            {
                StubSentPacket nextAnchorControl = TransferSent(
                    anchorTransports[anchorIndex], anchors[anchorIndex],
                    anchorTransports[anchorIndex + 1U]);
                RangeControlData forwardedControl{};
                TEST_ASSERT_TRUE(codec.DecodeControl(nextAnchorControl.payload,
                    nextAnchorControl.payloadLength, session, sequence,
                    forwardedControl));
                TEST_ASSERT_EQUAL_UINT8(anchorIndex + 1U,
                    forwardedControl.anchorIndex);
                TEST_ASSERT_EQUAL_UINT16(
                    static_cast<uint16_t>((anchorIndex + 1U) * 2U + 1U),
                    forwardedControl.pairSequence);
                anchorControllers[anchorIndex + 1U]->Update();
                anchorControllers[anchorIndex]->Update();
            }

            StubSentPacket measurement = TransferSent(
                anchorTransports[anchorIndex], anchors[anchorIndex],
                masterTransport);
            RangeMeasurementData decodedMeasurement{};
            TEST_ASSERT_TRUE(codec.DecodeMeasurement(measurement.payload,
                measurement.payloadLength,
                EnSequentialRangingPacketType::RangeMeasurement,
                session, sequence, decodedMeasurement));
            TEST_ASSERT_EQUAL_UINT8(anchorIndex,
                decodedMeasurement.anchorIndex);
            TEST_ASSERT_EQUAL_UINT8(tagIndex, decodedMeasurement.tagIndex);
            master.Update();
            TimedRangeMeasurement masterEvent{};
            TEST_ASSERT_TRUE(master.TryTakeMeasurement(masterEvent));
            TEST_ASSERT_EQUAL_UINT8(expectedAnchorIds[masterEventIndex],
                masterEvent.anchorId);
            TEST_ASSERT_EQUAL_UINT8(expectedTagIds[masterEventIndex],
                masterEvent.tagId);
            ++masterEventIndex;

            if (tagIndex == 1U && anchorIndex < 2U)
            {
                TransferSent(masterTransport, tags[0], followerTransport);
                follower.Update();
                TimedRangeMeasurement followerEvent{};
                TEST_ASSERT_TRUE(follower.TryTakeMeasurement(followerEvent));
                TEST_ASSERT_EQUAL_UINT8(2, followerEvent.tagId);
                ++followerEventCount;
            }
            else if (tagIndex == 1U && anchorIndex == 2U)
            {
                TEST_ASSERT_TRUE(masterTransport.TakeSent(nextRoundControl));
                RangeControlData roundTwo{};
                TEST_ASSERT_TRUE(codec.DecodeControl(nextRoundControl.payload,
                    nextRoundControl.payloadLength, session, sequence,
                    roundTwo));
                TEST_ASSERT_EQUAL_UINT32(2, roundTwo.roundId);
                TEST_ASSERT_EQUAL_UINT8(0, roundTwo.anchorIndex);
            }

            if (masterEventIndex < 6U)
            {
                SequentialRangeRoundSummary incomplete{};
                TEST_ASSERT_FALSE(master.TryTakeCompletedRound(incomplete));
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT8(6, masterEventIndex);
    SequentialRangeRoundSummary masterSummary{};
    TEST_ASSERT_TRUE(master.TryTakeCompletedRound(masterSummary));
    TEST_ASSERT_EQUAL_UINT8(6, masterSummary.receivedMeasurementCount);
    TEST_ASSERT_FALSE(masterSummary.timedOut);

    anchorThree.Update();
    StubSentPacket anchorComplete{};
    TEST_ASSERT_TRUE(anchorTransports[2].TakeSent(anchorComplete));
    RangeRoundCompleteData decodedAnchorComplete{};
    TEST_ASSERT_TRUE(codec.DecodeRoundComplete(anchorComplete.payload,
        anchorComplete.payloadLength, session, sequence,
        decodedAnchorComplete));
    TEST_ASSERT_EQUAL_UINT32(1, decodedAnchorComplete.roundId);

    master.Update();
    TransferSent(masterTransport, tags[0], followerTransport);
    follower.Update();
    TimedRangeMeasurement finalFollowerEvent{};
    TEST_ASSERT_TRUE(follower.TryTakeMeasurement(finalFollowerEvent));
    TEST_ASSERT_EQUAL_UINT8(30, finalFollowerEvent.anchorId);
    ++followerEventCount;

    master.Update();
    StubSentPacket followerComplete = TransferSent(
        masterTransport, tags[0], followerTransport);
    RangeRoundCompleteData decodedFollowerComplete{};
    TEST_ASSERT_TRUE(codec.DecodeRoundComplete(followerComplete.payload,
        followerComplete.payloadLength, session, sequence,
        decodedFollowerComplete));
    follower.Update();
    SequentialRangeRoundSummary followerSummary{};
    TEST_ASSERT_TRUE(follower.TryTakeCompletedRound(followerSummary));
    TEST_ASSERT_EQUAL_UINT32(1, followerSummary.roundId);
    TEST_ASSERT_EQUAL_UINT8(3, followerEventCount);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(TestFollowerDoesNotStartRound);
    RUN_TEST(TestFollowerRejectsReorderedAndDuplicateRoundComplete);
    RUN_TEST(TestOneAnchorOneTagRunsTwoRoundsContinuously);
    RUN_TEST(TestRoundCompletionWaitsForNewSynchronization);
    RUN_TEST(TestBusyDrainDefersNextRoundWithoutStartFailure);
    RUN_TEST(TestSessionChangeDiscardsStaleResultBeforeNewControlStarts);
    RUN_TEST(TestNewRoundAcceptsControlFromChangedSourceWithLowerSequence);
    RUN_TEST(TestTwoAnchorTwoTagOrderAndAnchorForwarding);
    RUN_TEST(TestMasterPublishesEachTwoByTwoMeasurementInOrder);
    RUN_TEST(TestDuplicateWrongSourceAndWrongSessionAreIgnored);
    RUN_TEST(TestForeignPacketIsNotConsumed);
    RUN_TEST(TestMasterChangeClearsQueuedEventsAndRound);
    RUN_TEST(TestNtpConversionFailurePublishesUnsynchronizedQuality);
    RUN_TEST(TestRoundTimeoutPublishesFailureSummary);
    RUN_TEST(TestThreeAnchorTwoTagConnectedEndToEndFlow);
    return UNITY_END();
}
