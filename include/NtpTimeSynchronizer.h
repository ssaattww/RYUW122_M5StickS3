#pragma once

#include <cstddef>
#include <cstdint>

#include "NtpTimeProtocolCodec.h"

class ConfigRuntime;
class EspNowBroadcast;
class EspNowTransport;
class TagMasterCoordinator;
struct EspNowReceivedPacket;

/**
 * @brief NTP四時刻から計算した1回分の同期サンプルを表します。
 */
struct NtpTimeSample
{
    int64_t nodeMinusMasterUs = 0;
    uint32_t roundTripUs = 0;
    int8_t rssi = 0;
    uint8_t channel = 0;
    EnTimeQuality timeQuality = EnTimeQuality::Unsynchronized;
    bool isValid = false;
};

/**
 * @brief ノード別に採用した時刻同期情報を表します。
 */
struct NodeTimeSynchronization
{
    uint8_t nodeId = 0;
    int64_t nodeMinusMasterUs = 0;
    uint32_t roundTripUs = 0;
    uint64_t synchronizedAtMasterTimeUs = 0;
    uint64_t synchronizationAgeUs = 0;
    int8_t rssi = 0;
    uint8_t channel = 0;
    EnTimeQuality timeQuality = EnTimeQuality::Unsynchronized;
    bool isValid = false;
};

/**
 * @brief 単調増加マイクロ秒時刻を返す関数を表します。
 *
 * @return 現在の単調増加マイクロ秒時刻
 */
using NtpTimeProvider = uint64_t (*)();

/**
 * @brief マスターTAGと各ノードのNTP四時刻同期を管理します。
 */
class NtpTimeSynchronizer
{
public:
    static constexpr size_t m_maxTargetCount = 16;
    static constexpr size_t m_sampleCountPerNode = 3;
    static constexpr uint64_t m_responseTimeoutUs = 100000;

    /**
     * @brief 共通transportと選出済みマスター情報を使用する同期管理を生成します。
     *
     * @param transport 共有するESP-NOW transport
     * @param broadcast 自端末状態と有効ノード一覧の提供元
     * @param coordinator マスターTAG識別情報の提供元
     * @param configRuntime Wi-Fiチャンネルと省電力設定の提供元
     * @param timeProvider 単調増加マイクロ秒時刻の提供関数
     */
    NtpTimeSynchronizer(
        EspNowTransport& transport,
        EspNowBroadcast& broadcast,
        TagMasterCoordinator& coordinator,
        ConfigRuntime& configRuntime,
        NtpTimeProvider timeProvider = nullptr);

    /**
     * @brief NTP受信、要求、応答、同期確定通知を非同期に進めます。
     */
    void Update();

    /**
     * @brief 現在のマスターセッションについて同期処理が完了したか確認します。
     *
     * @return マスターで全対象処理済み、または非マスターで自ノード同期済みの場合はtrue
     */
    bool IsSynchronizationComplete() const;

    /**
     * @brief 現在のローカル単調時刻に対応するマスターTAG基準時刻を取得します。
     *
     * @param masterTimeUs 現在のマスターTAG基準時刻格納先
     * @return 自ノードがマスター、または同期済み非マスターの場合はtrue。それ以外はfalse
     */
    bool TryGetCurrentMasterTime(uint64_t& masterTimeUs) const;

    /**
     * @brief 指定ノードの採用済み同期情報を取得します。
     *
     * @param nodeId 対象ノードID
     * @param synchronization 同期情報の格納先
     * @return 採用済み同期情報が存在する場合はtrue、それ以外はfalse
     */
    bool TryGetNodeSynchronization(
        uint8_t nodeId,
        NodeTimeSynchronization& synchronization) const;

    /**
     * @brief ノードの32bitローカル時刻を参照時刻に近いマスター64bit時刻へ変換します。
     *
     * @param nodeId 対象ノードID
     * @param nodeLocalTimeUs 対象ノードの32bitローカル時刻
     * @param referenceMasterTimeUs 変換結果を近づけるマスター64bit参照時刻
     * @param masterTimeUs 変換後のマスター64bit時刻格納先
     * @return 同期済みで変換できた場合はtrue、それ以外はfalse
     */
    bool TryConvertNodeTimeToMaster(
        uint8_t nodeId,
        uint32_t nodeLocalTimeUs,
        uint64_t referenceMasterTimeUs,
        uint64_t& masterTimeUs) const;

    /**
     * @brief 非マスターノード自身の32bitローカル時刻をマスター64bit時刻へ変換します。
     *
     * @param localTimeUs 自ノードの32bitローカル時刻
     * @param masterTimeUs 変換後のマスター64bit時刻格納先
     * @return 同期確定通知を受信済みで変換できた場合はtrue、それ以外はfalse
     */
    bool TryConvertLocalTimeToMaster(
        uint32_t localTimeUs,
        uint64_t& masterTimeUs) const;

    /**
     * @brief 32bit時計間のmodulo差分を符号付き64bitで返します。
     *
     * @param later 差分の左辺となる32bit時刻
     * @param earlier 差分の右辺となる32bit時刻
     * @return 折り返しを考慮したlater - earlier
     */
    static int64_t ModuloDifference(uint32_t later, uint32_t earlier);

    /**
     * @brief NTP四時刻からoffsetと往復遅延を計算します。
     *
     * @param t1 マスターTAGの要求送信直前時刻
     * @param t2 対象ノードの要求受信時刻
     * @param t3 対象ノードの応答送信直前時刻
     * @param t4 マスターTAGの応答受信時刻
     * @param sample 計算した同期サンプル格納先
     * @return 往復遅延が妥当な場合はtrue、それ以外はfalse
     */
    static bool CalculateSample(
        uint32_t t1,
        uint32_t t2,
        uint32_t t3,
        uint32_t t4,
        NtpTimeSample& sample);

    /**
     * @brief 有効な同期サンプルから往復遅延が最小の1件を選択します。
     *
     * @param samples 選択対象サンプル配列
     * @param sampleCount サンプル件数
     * @param selected 選択したサンプル格納先
     * @return 有効なサンプルが存在する場合はtrue、それ以外はfalse
     */
    static bool SelectBestSample(
        const NtpTimeSample* samples,
        size_t sampleCount,
        NtpTimeSample& selected);

    /**
     * @brief 32bit時刻を参照64bit時刻に最も近いepochへ拡張します。
     *
     * @param timestampUs 拡張する32bit時刻
     * @param referenceTimeUs 近傍を選ぶ64bit参照時刻
     * @return 参照時刻に最も近い64bit時刻
     */
    static uint64_t ExtendTimestampNear(
        uint32_t timestampUs,
        uint64_t referenceTimeUs);

    /**
     * @brief 省電力設定と両端の受信timestamp有無から時刻品質を決定します。
     *
     * @param localPowerSaveEnabled 自ノードのWi-Fi省電力が有効な場合はtrue
     * @param remotePowerSaveEnabled 対象ノードのWi-Fi省電力が有効な場合はtrue
     * @param remoteReceiveTimestampAvailable t2がrx_ctrl timestampの場合はtrue
     * @param localReceiveTimestampAvailable t4がrx_ctrl timestampの場合はtrue
     * @return 条件に対応する時刻品質
     */
    static EnTimeQuality ResolveTimeQuality(
        bool localPowerSaveEnabled,
        bool remotePowerSaveEnabled,
        bool remoteReceiveTimestampAvailable,
        bool localReceiveTimestampAvailable);

private:
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
     * @brief マスターが同期する1ノード分の進行状態を表します。
     */
    struct TargetState
    {
        uint8_t nodeId = 0;
        uint8_t macAddress[6]{};
        uint8_t attemptCount = 0;
        uint8_t validSampleCount = 0;
        NtpTimeSample samples[m_sampleCountPerNode]{};
        NodeTimeSynchronization synchronization{};
        bool commitPending = false;
        bool completed = false;
    };

    /**
     * @brief 対象ノードで応答送信を待つ同期要求を表します。
     */
    struct PendingResponse
    {
        uint8_t destinationMac[6]{};
        uint32_t sessionId = 0;
        uint32_t sequence = 0;
        uint32_t t1 = 0;
        uint32_t t2 = 0;
        bool receiveTimestampAvailable = false;
        bool isPending = false;
    };

    /**
     * @brief ESP timerを使用する既定時刻提供関数です。
     *
     * @return 現在の単調増加マイクロ秒時刻
     */
    static uint64_t DefaultTimeProvider();

    /**
     * @brief coordinatorのマスター識別変化を検出して旧同期状態を破棄します。
     */
    void DetectMasterChange();

    /**
     * @brief 現在のマスターセッションに属する同期状態を全破棄します。
     */
    void ResetSynchronizationState();

    /**
     * @brief 有効NodeStatusから未処理の同期対象をノードID昇順で追加します。
     */
    void DiscoverNewTargets();

    /**
     * @brief ノードIDまたはMACアドレスが現在セッションで処理済みか確認します。
     *
     * @param nodeId 確認するノードID
     * @param macAddress 確認するMACアドレス
     * @return 既に同期対象へ登録済みの場合はtrue、それ以外はfalse
     */
    bool IsTargetTracked(
        uint8_t nodeId,
        const uint8_t macAddress[6]) const;

    /**
     * @brief 対応する2つの64bit時刻を基準に別時計domainへ変換します。
     *
     * @param sourceTimeUs 変換するsource時計の64bit時刻
     * @param sourceAnchorUs 対応点となるsource時計の64bit時刻
     * @param destinationAnchorUs 対応点となるdestination時計の64bit時刻
     * @param destinationTimeUs 変換後の64bit時刻格納先
     * @return 64bit範囲内で変換できた場合はtrue、それ以外はfalse
     */
    static bool TranslateClockDomain(
        uint64_t sourceTimeUs,
        uint64_t sourceAnchorUs,
        uint64_t destinationAnchorUs,
        uint64_t& destinationTimeUs);

    /**
     * @brief transport FIFO先頭にあるNTP packetだけを処理します。
     */
    void ProcessReceivedPackets();

    /**
     * @brief 受信した同期要求を検証して応答待ち状態へ保存します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleRequest(const EspNowReceivedPacket& packet);

    /**
     * @brief 受信した同期応答を検証して現在ノードのサンプルへ追加します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleResponse(const EspNowReceivedPacket& packet);

    /**
     * @brief 受信した同期確定通知を検証して自ノードの変換情報へ反映します。
     *
     * @param packet transportから取得した受信packet
     */
    void HandleCommit(const EspNowReceivedPacket& packet);

    /**
     * @brief transportがidleの場合に対象ノードの同期応答を送信します。
     */
    void TrySendPendingResponse();

    /**
     * @brief マスターTAGとして対象ノードを1件ずつ同期します。
     */
    void UpdateMaster();

    /**
     * @brief 現在対象へ次の同期要求を送信します。
     */
    void TrySendRequest();

    /**
     * @brief 現在対象の有効サンプルから同期情報を確定します。
     */
    void FinalizeCurrentTarget();

    /**
     * @brief 全非マスターノードへ採用済み同期情報を通知します。
     */
    void TrySendCommit();

    /**
     * @brief packet送信用の非0シーケンスを生成します。
     *
     * @return 非0の新しいシーケンス
     */
    uint32_t NextSequence();

    EspNowTransport& m_transport;
    EspNowBroadcast& m_broadcast;
    TagMasterCoordinator& m_coordinator;
    ConfigRuntime& m_configRuntime;
    NtpTimeProvider m_timeProvider;
    MasterState m_master{};
    TargetState m_targets[m_maxTargetCount]{};
    size_t m_targetCount = 0;
    size_t m_currentTargetIndex = 0;
    PendingResponse m_pendingResponse{};
    NodeTimeSynchronization m_localSynchronization{};
    uint64_t m_localSynchronizationAnchorUs = 0;
    uint32_t m_nextSequence = 0;
    uint32_t m_pendingRequestSequence = 0;
    uint32_t m_pendingRequestT1 = 0;
    uint64_t m_requestStartedUs = 0;
    bool m_requestPending = false;
};

static_assert(
    NtpTimeSynchronizer::m_sampleCountPerNode == 3,
    "NTP synchronization requires three samples per node");
static_assert(
    NtpTimeSynchronizer::m_responseTimeoutUs == 100000,
    "NTP response timeout must remain 100ms");
