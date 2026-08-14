#include <unity.h>

#include "SequentialRangingProtocolCodec.h"

#include <cstring>

namespace
{
    /**
     * @brief test用MACアドレスを生成します。
     *
     * @param tail MACアドレス末尾
     * @param address 生成先MACアドレス
     */
    void SetMac(uint8_t tail, uint8_t address[6])
    {
        memset(address, 0, 6);
        address[0] = 0x02;
        address[5] = tail;
    }

    /**
     * @brief test用ノード識別情報を生成します。
     *
     * @param nodeId ノードID
     * @param macTail MACアドレス末尾
     * @param address UWBアドレス
     * @return 生成したノード識別情報
     */
    RangingNodeIdentity MakeIdentity(uint8_t nodeId, uint8_t macTail,
        const char* address)
    {
        RangingNodeIdentity identity{};
        identity.nodeId = nodeId;
        SetMac(macTail, identity.macAddress);
        memcpy(identity.uwbAddress, address, 9);
        return identity;
    }

    /**
     * @brief test用測距制御を生成します。
     *
     * @return 生成した測距制御
     */
    RangeControlData MakeControl()
    {
        RangeControlData data{};
        data.roundId = 11;
        data.pairSequence = 4;
        data.masterTagId = 1;
        SetMac(1, data.masterMac);
        data.anchorCount = 2;
        data.tagCount = 2;
        data.anchorIndex = 1;
        data.tagIndex = 1;
        data.anchorIds[0] = 10;
        data.anchorIds[1] = 20;
        data.tagIds[0] = 1;
        data.tagIds[1] = 2;
        return data;
    }

    /**
     * @brief test用逐次測距結果を生成します。
     *
     * @return 生成した逐次測距結果
     */
    RangeMeasurementData MakeMeasurement()
    {
        RangeMeasurementData data{};
        data.roundId = 11;
        data.pairSequence = 4;
        data.masterTagId = 1;
        SetMac(1, data.masterMac);
        data.anchorCount = 2;
        data.tagCount = 2;
        data.anchorIndex = 1;
        data.tagIndex = 1;
        data.anchor = MakeIdentity(20, 20, "A0000020");
        data.tag = MakeIdentity(2, 2, "T0000002");
        data.status = EnRangeResultStatus::Success;
        data.distanceMm = 123456;
        data.uwbRssi = -77;
        data.commandReceivedUs = 100;
        data.rangingStartedUs = 120;
        data.rangingCompletedUs = 220;
        data.espNowRssi = -42;
        data.isLastMeasurement = true;
        return data;
    }

    /**
     * @brief test用ラウンド完了情報を生成します。
     *
     * @return 生成したラウンド完了情報
     */
    RangeRoundCompleteData MakeComplete()
    {
        RangeRoundCompleteData data{};
        data.roundId = 11;
        data.nextRoundId = 12;
        data.masterTagId = 1;
        SetMac(1, data.masterMac);
        data.startedMasterTimeUs = 1000;
        data.completedMasterTimeUs = 2000;
        data.anchorCount = 2;
        data.tagCount = 2;
        data.expectedMeasurementCount = 4;
        data.receivedMeasurementCount = 3;
        data.missingMeasurementBits = UINT64_C(1) << 2;
        data.timedOut = true;
        return data;
    }

    /**
     * @brief 測距制御のroundtripと最大境界を検証します。
     */
    void TestControlRoundTripAndMaximumBoundary()
    {
        RangeControlData input = MakeControl();
        input.anchorCount = 8;
        input.tagCount = 8;
        input.anchorIndex = 7;
        input.tagIndex = 7;
        input.pairSequence = 64;
        for (uint8_t index = 0; index < 8; ++index)
        {
            input.anchorIds[index] = static_cast<uint8_t>(10U + index);
            input.tagIds[index] = static_cast<uint8_t>(1U + index);
        }
        RangeControlPacket packet{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, packet));
        TEST_ASSERT_TRUE(sizeof(packet) <= 250U);
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::IsSequentialRangingPacket(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet)));
        uint32_t session = 0;
        uint32_t sequence = 0;
        RangeControlData output{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT32(5, session);
        TEST_ASSERT_EQUAL_UINT32(9, sequence);
        TEST_ASSERT_EQUAL_UINT8(8, output.anchorCount);
        TEST_ASSERT_EQUAL_UINT16(64, output.pairSequence);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(input.anchorIds, output.anchorIds, 8);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(input.tagIds, output.tagIds, 8);
    }

    /**
     * @brief 測距結果と時刻変換済み転送のroundtripを検証します。
     */
    void TestMeasurementAndForwardRoundTrip()
    {
        RangeMeasurementData input = MakeMeasurement();
        RangeMeasurementPacket packet{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        uint32_t session = 0;
        uint32_t sequence = 0;
        RangeMeasurementData output{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::DecodeMeasurement(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            EnSequentialRangingPacketType::RangeMeasurement,
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT32(input.distanceMm, output.distanceMm);
        TEST_ASSERT_EQUAL_INT16(input.uwbRssi, output.uwbRssi);
        TEST_ASSERT_EQUAL_STRING(input.tag.uwbAddress, output.tag.uwbAddress);
        TEST_ASSERT_TRUE(output.isLastMeasurement);

        input.commandReceivedMasterTimeUs = UINT64_C(10000000000);
        input.rangingStartedMasterTimeUs = UINT64_C(10000000020);
        input.rangingCompletedMasterTimeUs = UINT64_C(10000000120);
        input.synchronizationRoundTripUs = 80;
        input.synchronizationAgeUs = UINT64_C(5000000000);
        input.timeQuality = EnTimeQuality::Synchronized;
        TEST_ASSERT_TRUE(
            SequentialRangingProtocolCodec::EncodeMeasurementForward(
                5, 11, input, packet));
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::DecodeMeasurement(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            EnSequentialRangingPacketType::RangeMeasurementForward,
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT64(input.rangingStartedMasterTimeUs,
            output.rangingStartedMasterTimeUs);
        TEST_ASSERT_EQUAL_UINT64(input.rangingCompletedMasterTimeUs,
            output.rangingCompletedMasterTimeUs);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(input.timeQuality),
            static_cast<uint8_t>(output.timeQuality));
    }

    /**
     * @brief ラウンド完了情報のroundtripと64組境界を検証します。
     */
    void TestRoundCompleteRoundTripAndMaximumBoundary()
    {
        RangeRoundCompleteData input = MakeComplete();
        input.anchorCount = 8;
        input.tagCount = 8;
        input.expectedMeasurementCount = 64;
        input.receivedMeasurementCount = 63;
        input.missingMeasurementBits = UINT64_C(1) << 63;
        RangeRoundCompletePacket packet{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeRoundComplete(
            5, 12, input, packet));
        uint32_t session = 0;
        uint32_t sequence = 0;
        RangeRoundCompleteData output{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::DecodeRoundComplete(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT8(64, output.expectedMeasurementCount);
        TEST_ASSERT_EQUAL_UINT64(input.missingMeasurementBits,
            output.missingMeasurementBits);
    }

    /**
     * @brief header、長さ、種別、session、sequence不正を拒否することを検証します。
     */
    void TestInvalidHeaderSizeTypeAndIdentifiers()
    {
        RangeControlData input = MakeControl();
        RangeControlPacket packet{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, packet));
        uint32_t session = 77;
        uint32_t sequence = 88;
        RangeControlData output = MakeControl();
        output.roundId = 99;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet) - 1U,
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT32(77, session);
        TEST_ASSERT_EQUAL_UINT32(88, sequence);
        TEST_ASSERT_EQUAL_UINT32(99, output.roundId);

        const RangeControlPacket original = packet;
        packet.header.magic = 0;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        packet = original;
        packet.header.version++;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        packet = original;
        packet.header.packetType = static_cast<uint8_t>(
            EnSequentialRangingPacketType::RangeMeasurement);
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        packet = original;
        packet.header.sessionId = 0;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        packet = original;
        packet.header.sequence = 0;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeControl(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
    }

    /**
     * @brief 制御packetの件数、index、ID、MAC不正とencode出力不変を検証します。
     */
    void TestInvalidControlFieldsPreserveOutput()
    {
        RangeControlData input = MakeControl();
        RangeControlPacket output{};
        memset(&output, 0x5a, sizeof(output));
        RangeControlPacket before = output;
        input.anchorCount = 0;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, output));
        TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
        input = MakeControl();
        input.anchorIndex = input.anchorCount;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, output));
        input = MakeControl();
        input.pairSequence = 3;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, output));
        input = MakeControl();
        input.tagIds[1] = input.anchorIds[0];
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, output));
        input = MakeControl();
        memset(input.masterMac, 0, sizeof(input.masterMac));
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeControl(
            5, 9, input, output));
    }

    /**
     * @brief 測距結果のenum、address、時刻、結果値不正を拒否します。
     */
    void TestInvalidMeasurementFields()
    {
        RangeMeasurementData input = MakeMeasurement();
        RangeMeasurementPacket packet{};
        input.status = static_cast<EnRangeResultStatus>(99);
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        input = MakeMeasurement();
        input.tag.uwbAddress[0] = ' ';
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        input = MakeMeasurement();
        input.rangingStartedUs = 300;
        input.rangingCompletedUs = 200;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        input = MakeMeasurement();
        input.status = EnRangeResultStatus::TimedOut;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        input = MakeMeasurement();
        input.commandReceivedMasterTimeUs = 1;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        input = MakeMeasurement();
        input.timeQuality = static_cast<EnTimeQuality>(99);
        TEST_ASSERT_FALSE(
            SequentialRangingProtocolCodec::EncodeMeasurementForward(
                5, 10, input, packet));
    }

    /**
     * @brief 32bitローカル時刻の折り返し境界を受け付けることを検証します。
     */
    void TestLocalTimeWrapBoundary()
    {
        RangeMeasurementData input = MakeMeasurement();
        input.commandReceivedUs = UINT32_MAX - 20U;
        input.rangingStartedUs = UINT32_MAX - 10U;
        input.rangingCompletedUs = 10U;
        RangeMeasurementPacket packet{};
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
    }

    /**
     * @brief ラウンド件数、bitset、時刻、bool不正とdecode出力不変を検証します。
     */
    void TestInvalidRoundCompleteFieldsPreserveOutput()
    {
        RangeRoundCompleteData input = MakeComplete();
        RangeRoundCompletePacket packet{};
        input.expectedMeasurementCount = 3;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeRoundComplete(
            5, 12, input, packet));
        input = MakeComplete();
        input.missingMeasurementBits = UINT64_C(1) << 10;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeRoundComplete(
            5, 12, input, packet));
        input = MakeComplete();
        input.startedMasterTimeUs = 3000;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeRoundComplete(
            5, 12, input, packet));

        input = MakeComplete();
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeRoundComplete(
            5, 12, input, packet));
        packet.timedOut = 2;
        uint32_t session = 77;
        uint32_t sequence = 88;
        RangeRoundCompleteData output = MakeComplete();
        output.roundId = 99;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeRoundComplete(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT32(77, session);
        TEST_ASSERT_EQUAL_UINT32(88, sequence);
        TEST_ASSERT_EQUAL_UINT32(99, output.roundId);
    }

    /**
     * @brief 測距結果のencodeとdecodeが失敗時に出力を変更しないことを検証します。
     */
    void TestMeasurementFailurePreservesOutputs()
    {
        RangeMeasurementData input = MakeMeasurement();
        RangeMeasurementPacket packet{};
        memset(&packet, 0x5a, sizeof(packet));
        const RangeMeasurementPacket before = packet;
        input.roundId = 0;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        TEST_ASSERT_EQUAL_MEMORY(&before, &packet, sizeof(packet));

        input = MakeMeasurement();
        TEST_ASSERT_TRUE(SequentialRangingProtocolCodec::EncodeMeasurement(
            5, 10, input, packet));
        packet.status = 99;
        uint32_t session = 77;
        uint32_t sequence = 88;
        RangeMeasurementData output = MakeMeasurement();
        output.roundId = 99;
        TEST_ASSERT_FALSE(SequentialRangingProtocolCodec::DecodeMeasurement(
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet),
            EnSequentialRangingPacketType::RangeMeasurement,
            session, sequence, output));
        TEST_ASSERT_EQUAL_UINT32(77, session);
        TEST_ASSERT_EQUAL_UINT32(88, sequence);
        TEST_ASSERT_EQUAL_UINT32(99, output.roundId);
    }
}

/**
 * @brief 各test前の初期化を行います。
 */
void setUp()
{
}

/**
 * @brief 各test後の後処理を行います。
 */
void tearDown()
{
}

/**
 * @brief T-006 codec testを実行します。
 *
 * @return test終了コード
 */
int main()
{
    UNITY_BEGIN();
    RUN_TEST(TestControlRoundTripAndMaximumBoundary);
    RUN_TEST(TestMeasurementAndForwardRoundTrip);
    RUN_TEST(TestRoundCompleteRoundTripAndMaximumBoundary);
    RUN_TEST(TestInvalidHeaderSizeTypeAndIdentifiers);
    RUN_TEST(TestInvalidControlFieldsPreserveOutput);
    RUN_TEST(TestInvalidMeasurementFields);
    RUN_TEST(TestLocalTimeWrapBoundary);
    RUN_TEST(TestInvalidRoundCompleteFieldsPreserveOutput);
    RUN_TEST(TestMeasurementFailurePreservesOutputs);
    return UNITY_END();
}
