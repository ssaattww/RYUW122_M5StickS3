#pragma once

#include <cstddef>
#include <cstdint>

#include "SequentialRangingProtocolCodec.h"

class EspNowBroadcast;
class EspNowTransport;
class NtpTimeSynchronizer;
class Ryuw122Controller;
class TagMasterCoordinator;
struct EspNowReceivedPacket;
struct NodeStatus;
struct Ryuw122RangingResult;

/**
 * @brief 逐次測距コントローラーの役割別状態を表します。
 */
enum class EnSequentialRangingState : uint8_t
{
    WaitingForMaster,
    FollowingMaster,
    WaitingForSynchronization,
    ReadyToStart,
    RunningRound,
    AnchorIdle,
    AnchorRanging,
};

/**
 * @brief マスターTAG基準時刻へ変換した逐次測距結果を表します。
 */
struct TimedRangeMeasurement
{
    uint32_t sessionId = 0;
    uint32_t roundId = 0;
    uint8_t anchorId = 0;
    uint8_t tagId = 0;
    EnRangeResultStatus status = EnRangeResultStatus::Failed;
    uint32_t distanceMm = 0;
    int16_t uwbRssi = 0;
    int8_t espNowRssi = 0;
    uint64_t commandReceivedMasterTimeUs = 0;
    uint64_t rangingStartedMasterTimeUs = 0;
    uint64_t rangingCompletedMasterTimeUs = 0;
    uint32_t rangingDurationUs = 0;
    uint32_t synchronizationRoundTripUs = 0;
    uint64_t synchronizationAgeUs = 0;
    EnTimeQuality timeQuality = EnTimeQuality::Unsynchronized;
    bool isLastMeasurement = false;
};

/**
 * @brief 完了した逐次測距ラウンドの統計を表します。
 */
struct SequentialRangeRoundSummary
{
    uint32_t sessionId = 0;
    uint32_t roundId = 0;
    uint64_t startedMasterTimeUs = 0;
    uint64_t completedMasterTimeUs = 0;
    uint32_t totalDurationUs = 0;
    uint8_t anchorCount = 0;
    uint8_t tagCount = 0;
    uint8_t expectedMeasurementCount = 0;
    uint8_t receivedMeasurementCount = 0;
    bool anchorListTruncated = false;
    bool tagListTruncated = false;
    bool timedOut = false;
};

/**
 * @brief 逐次測距コントローラーの診断件数を表します。
 */
struct SequentialRangingDiagnostics
{
    uint32_t measurementQueueOverflowCount = 0;
    uint32_t roundQueueOverflowCount = 0;
    uint32_t outboundQueueOverflowCount = 0;
    uint32_t invalidPacketCount = 0;
    uint32_t duplicatePacketCount = 0;
    uint32_t sendFailureCount = 0;
};

/**
 * @brief 単調増加マイクロ秒時刻を返す関数を表します。
 *
 * @return 現在の単調増加マイクロ秒時刻
 */
using SequentialRangingTimeProvider = uint64_t (*)();

/**
 * @brief TAGとANCHORの最短周期逐次測距状態機械を管理します。
 */
class SequentialRangingController
{
public:
    static constexpr uint32_t m_uwbTimeoutUs = 300000U;
    static constexpr uint32_t m_espNowHopBudgetUs = 50000U;
    static constexpr uint32_t m_finalReturnBudgetUs = 50000U;
    static constexpr size_t m_measurementQueueCapacity = 64U;
    static constexpr size_t m_roundQueueCapacity = 4U;

    /**
     * @brief 共有通信、同期、測距依存を注入してコントローラーを生成します。
     *
     * @param transport ESP-NOW送受信transport
     * @param broadcast 自ノード状態と受信ノード一覧
     * @param coordinator TAGマスター選出結果
     * @param synchronizer ノード時刻同期と変換処理
     * @param ryuw122 非同期UWB測距処理
     * @param codec 逐次測距packet codec
     * @param timeProvider 単調増加マイクロ秒時刻。nullptrの場合はESP timerを使用
     */
    SequentialRangingController(
        EspNowTransport& transport,
        EspNowBroadcast& broadcast,
        TagMasterCoordinator& coordinator,
        NtpTimeSynchronizer& synchronizer,
        Ryuw122Controller& ryuw122,
        SequentialRangingProtocolCodec& codec,
        SequentialRangingTimeProvider timeProvider = nullptr);

    /**
     * @brief 現在の役割を初期化し、旧セッション状態を破棄します。
     */
    void Begin();

    /**
     * @brief 受信packet、UWB測距、timeout、送信要求を非同期に進めます。
     */
    void Update();

    /**
     * @brief 公開待ちの逐次測距結果を1件取得します。
     *
     * @param measurement 取得した逐次測距結果の格納先
     * @return 逐次測距結果を取得した場合はtrue、それ以外はfalse
     */
    bool TryTakeMeasurement(TimedRangeMeasurement& measurement);

    /**
     * @brief 公開待ちのラウンド完了情報を1件取得します。
     *
     * @param summary 取得したラウンド完了情報の格納先
     * @return ラウンド完了情報を取得した場合はtrue、それ以外はfalse
     */
    bool TryTakeCompletedRound(SequentialRangeRoundSummary& summary);

    /**
     * @brief 現在の役割別状態を取得します。
     *
     * @return 現在の状態
     */
    EnSequentialRangingState GetState() const;

    /**
     * @brief 現在の診断件数を取得します。
     *
     * @return 現在の診断件数
     */
    SequentialRangingDiagnostics GetDiagnostics() const;

private:
    static constexpr size_t m_maxNodeCount = 17U;
    static constexpr size_t m_highPriorityQueueCapacity = 4U;
    static constexpr size_t m_lowPriorityQueueCapacity = 80U;

    /**
     * @brief 比較用に保持するマスターセッション識別情報を表します。
     */
    struct MasterState
    {
        bool isValid = false;
        bool isSelfMaster = false;
        uint8_t nodeId = 0;
        uint8_t macAddress[6]{};
        uint32_t sessionId = 0;
    };

    /**
     * @brief 送信優先度FIFOへ保存する固定長packetを表します。
     */
    struct OutboundPacket
    {
        uint8_t destinationMac[6]{};
        uint16_t payloadLength = 0;
        uint8_t payload[sizeof(RangeMeasurementPacket)]{};
    };

    /**
     * @brief ESP timerを使用する既定時刻提供関数です。
     *
     * @return 現在の単調増加マイクロ秒時刻
     */
    static uint64_t DefaultTimeProvider();

    /**
     * @brief coordinatorのマスター変更を検出して旧ラウンドを破棄します。
     */
    void DetectMasterChange();

    /**
     * @brief セッションに属する状態、event、送信待ちを全破棄します。
     */
    void ResetSessionState();

    /**
     * @brief transport FIFO先頭の逐次測距packetだけを処理します。
     */
    void ProcessReceivedPackets();

    /**
     * @brief 測距制御packetを検証して自ANCHORの測距を開始します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleControl(const EspNowReceivedPacket& packet);

    /**
     * @brief ANCHOR測距結果packetを検証してマスターへ公開します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleMeasurement(const EspNowReceivedPacket& packet);

    /**
     * @brief マスター転送結果packetを検証してフォロワーへ公開します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleMeasurementForward(const EspNowReceivedPacket& packet);

    /**
     * @brief ラウンド完了packetを検証して経路完了または統計を反映します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleRoundComplete(const EspNowReceivedPacket& packet);

    /**
     * @brief マスターTAGの同期完了とround timeoutを処理します。
     */
    void UpdateMaster();

    /**
     * @brief ANCHORのRYUW122応答と次TAGへの遷移を処理します。
     */
    void UpdateAnchor();

    /**
     * @brief 有効かつ同期済みNodeStatusから新しい二重経路を構築します。
     *
     * @return ANCHORとTAGが1台以上存在する場合はtrue、それ以外はfalse
     */
    bool BuildRoundSnapshot();

    /**
     * @brief 新しいラウンドを作成して最小ANCHORへの制御を優先queueへ追加します。
     *
     * @return 新しいラウンドを開始要求できた場合はtrue、それ以外はfalse
     */
    bool StartMasterRound();

    /**
     * @brief 現在のマスターラウンドを完了し、次ラウンドを直ちに開始します。
     *
     * @param timedOut 欠損またはtimeoutで終了した場合はtrue
     */
    void CompleteMasterRound(bool timedOut);

    /**
     * @brief 現在位置のTAGへUWB測距を開始します。
     *
     * @return 測距開始要求を受け付けた場合はtrue、それ以外はfalse
     */
    bool StartCurrentAnchorRanging();

    /**
     * @brief 完了したUWB測距をwire結果へ変換して経路を次へ進めます。
     *
     * @param result 完了したUWB測距結果
     */
    void CompleteAnchorMeasurement(const Ryuw122RangingResult& result);

    /**
     * @brief 現在経路位置の測距制御を指定ANCHORへ送信待ちにします。
     *
     * @param anchorIndex 送信先ANCHOR index
     * @return 送信待ちへ追加できた場合はtrue、それ以外はfalse
     */
    bool QueueControl(uint8_t anchorIndex);

    /**
     * @brief ANCHORのローカル測距結果をマスターTAG宛て送信待ちにします。
     *
     * @param measurement 送信するローカル時刻domainの測距結果
     * @return 送信待ちへ追加できた場合はtrue、それ以外はfalse
     */
    bool QueueAnchorMeasurement(const RangeMeasurementData& measurement);

    /**
     * @brief 最終ANCHORからマスターTAGへ経路完了通知を送信待ちにします。
     *
     * @return 送信待ちへ追加できた場合はtrue、それ以外はfalse
     */
    bool QueueAnchorRoundComplete();

    /**
     * @brief マスター時刻へ変換済み結果を対象フォロワーへ送信待ちにします。
     *
     * @param measurement 転送する測距結果
     * @return 送信待ちへ追加できた場合はtrue、それ以外はfalse
     */
    bool QueueMeasurementForward(const RangeMeasurementData& measurement);

    /**
     * @brief 完了統計を全フォロワーTAGへ送信待ちにします。
     *
     * @param complete 送信するラウンド完了情報
     */
    void QueueRoundCompleteForFollowers(const RangeRoundCompleteData& complete);

    /**
     * @brief 優先queueからtransportへ1件だけ送信要求します。
     */
    void TrySendNextPacket();

    /**
     * @brief 固定長送信FIFOへpacketを追加します。
     *
     * @param queue 追加先FIFO
     * @param capacity FIFO容量
     * @param tail 末尾index
     * @param count 格納件数
     * @param destinationMac 送信先MACアドレス
     * @param payload 送信payload
     * @param payloadLength 送信payloadサイズ
     * @return 追加できた場合はtrue、それ以外はfalse
     */
    bool PushOutbound(
        OutboundPacket* queue,
        size_t capacity,
        size_t& tail,
        size_t& count,
        const uint8_t destinationMac[6],
        const void* payload,
        size_t payloadLength);

    /**
     * @brief 受信したANCHOR時刻domainの結果を公開形式へ変換します。
     *
     * @param measurement 受信した測距結果
     * @param timedMeasurement 変換後の公開結果格納先
     * @return 3時刻を同期済みmaster時刻へ変換できた場合はtrue
     */
    bool ConvertMeasurementToMaster(
        RangeMeasurementData& measurement,
        TimedRangeMeasurement& timedMeasurement);

    /**
     * @brief 測距結果を公開FIFOへ追加します。
     *
     * @param measurement 追加する測距結果
     * @return FIFOへ追加できた場合はtrue、それ以外はfalse
     */
    bool PushMeasurement(const TimedRangeMeasurement& measurement);

    /**
     * @brief ラウンド完了情報を公開FIFOへ追加します。
     *
     * @param summary 追加するラウンド完了情報
     * @return FIFOへ追加できた場合はtrue、それ以外はfalse
     */
    bool PushRoundSummary(const SequentialRangeRoundSummary& summary);

    /**
     * @brief 現在有効でIDが一意なNodeStatusを取得します。
     *
     * @param nodeId 取得するノードID
     * @param status 取得した状態の格納先
     * @return 一意で期限内のノードが存在する場合はtrue、それ以外はfalse
     */
    bool TryResolveNode(uint8_t nodeId, NodeStatus& status) const;

    /**
     * @brief NodeStatusをpacket用識別情報へ変換します。
     *
     * @param status 変換するノード状態
     * @return packet用識別情報
     */
    static RangingNodeIdentity BuildIdentity(const NodeStatus& status);

    /**
     * @brief packet送信用の非0シーケンスを生成します。
     *
     * @return 非0の新しいpacketシーケンス
     */
    uint32_t NextPacketSequence();

    EspNowTransport& m_transport;
    EspNowBroadcast& m_broadcast;
    TagMasterCoordinator& m_coordinator;
    NtpTimeSynchronizer& m_synchronizer;
    Ryuw122Controller& m_ryuw122;
    SequentialRangingProtocolCodec& m_codec;
    SequentialRangingTimeProvider m_timeProvider;
    MasterState m_master{};
    EnSequentialRangingState m_state =
        EnSequentialRangingState::WaitingForMaster;
    RangingNodeIdentity m_anchors[8]{};
    RangingNodeIdentity m_tags[8]{};
    uint8_t m_anchorCount = 0;
    uint8_t m_tagCount = 0;
    uint8_t m_anchorIndex = 0;
    uint8_t m_tagIndex = 0;
    uint32_t m_roundId = 0;
    uint64_t m_roundStartedUs = 0;
    uint64_t m_roundDeadlineUs = 0;
    uint64_t m_receivedMeasurementBits = 0;
    uint32_t m_lastMeasurementSequence[8]{};
    uint32_t m_lastForwardSequence[8]{};
    uint32_t m_lastControlRoundId = 0;
    uint16_t m_lastControlPairSequence = 0;
    uint32_t m_lastControlPacketSequence = 0;
    uint32_t m_lastCompleteSequence = 0;
    uint32_t m_lastCompletedRoundId = 0;
    uint32_t m_nextPacketSequence = 0;
    uint32_t m_anchorCommandReceivedUs = 0;
    bool m_anchorListTruncated = false;
    bool m_tagListTruncated = false;
    bool m_begun = false;
    TimedRangeMeasurement m_measurementQueue[m_measurementQueueCapacity]{};
    size_t m_measurementHead = 0;
    size_t m_measurementTail = 0;
    size_t m_measurementCount = 0;
    SequentialRangeRoundSummary m_roundQueue[m_roundQueueCapacity]{};
    size_t m_roundHead = 0;
    size_t m_roundTail = 0;
    size_t m_roundCount = 0;
    OutboundPacket m_highPriorityQueue[m_highPriorityQueueCapacity]{};
    size_t m_highPriorityHead = 0;
    size_t m_highPriorityTail = 0;
    size_t m_highPriorityCount = 0;
    OutboundPacket m_lowPriorityQueue[m_lowPriorityQueueCapacity]{};
    size_t m_lowPriorityHead = 0;
    size_t m_lowPriorityTail = 0;
    size_t m_lowPriorityCount = 0;
    SequentialRangingDiagnostics m_diagnostics{};
};

static_assert(
    SequentialRangingController::m_uwbTimeoutUs == 300000U,
    "UWB ranging timeout must remain 300ms");
