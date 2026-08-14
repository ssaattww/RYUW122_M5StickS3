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
    result.distanceMm = 1000;
    result.uwbRssi = -60;
    result.startedAtUs = 110;
    result.completedAtUs = 130;
    anchorRyuw.Complete(result);
    anchor.Update();
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
    controller.Update();
    RangeMeasurementData measurement = MakeMeasurement(tag, &anchor, 1,
        &tag, 1, 0, 0);
    transport.Inject(EncodeMeasurement(codec, 100, 10, measurement,
        tag.macAddress, anchor.macAddress));
    controller.Update();
    coordinator.SetMaster(MakeMaster(tag, 200), true);
    controller.Update();
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

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(TestFollowerDoesNotStartRound);
    RUN_TEST(TestOneAnchorOneTagRunsTwoRoundsContinuously);
    RUN_TEST(TestTwoAnchorTwoTagOrderAndAnchorForwarding);
    RUN_TEST(TestMasterPublishesEachTwoByTwoMeasurementInOrder);
    RUN_TEST(TestDuplicateWrongSourceAndWrongSessionAreIgnored);
    RUN_TEST(TestForeignPacketIsNotConsumed);
    RUN_TEST(TestMasterChangeClearsQueuedEventsAndRound);
    RUN_TEST(TestNtpConversionFailurePublishesUnsynchronizedQuality);
    RUN_TEST(TestRoundTimeoutPublishesFailureSummary);
    return UNITY_END();
}
