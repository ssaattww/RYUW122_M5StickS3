#pragma once

#include <array>
#include <cstdint>

#include "NodeStatus.h"

class EspNowBroadcast;

/**
 * @brief 選出されたマスターTAGの識別情報を表します。
 */
struct TagMasterIdentity
{
    bool isValid = false;
    uint8_t nodeID = 0;
    std::array<uint8_t, 6> macAddress{};
    uint32_t sessionId = 0;
};

/**
 * @brief マスター変更時に後続状態を破棄するための通知を表します。
 */
struct TagMasterChange
{
    TagMasterIdentity previousMaster{};
    TagMasterIdentity currentMaster{};
    bool requiresStateReset = true;
};

/**
 * @brief 有効なTAGから最小ノードIDのマスターを選出します。
 */
class TagMasterCoordinator
{
public:
    static constexpr uint32_t m_startupElectionWaitMs = 500;
    static constexpr uint32_t m_nodeExpirationMs = 30000;

    /**
     * @brief NodeStatus一覧を使用するマスター選出管理を生成します。
     *
     * @param broadcast 自ノード状態と受信ノード一覧の提供元
     */
    explicit TagMasterCoordinator(EspNowBroadcast& broadcast);

    /**
     * @brief 起動時選出待ちを開始し、マスター状態を初期化します。
     *
     * @param nowMs 現在の単調増加ミリ秒時刻
     */
    void Begin(uint32_t nowMs);

    /**
     * @brief 有効なTAG一覧を評価し、必要ならマスターを変更します。
     *
     * @param nowMs 現在の単調増加ミリ秒時刻
     */
    void Update(uint32_t nowMs);

    /**
     * @brief 起動時選出待ちが完了したか確認します。
     *
     * @return 500ms待機後に一度でも選出を評価した場合はtrue
     */
    bool IsElectionComplete() const;

    /**
     * @brief 有効なマスターTAGが選出済みか確認します。
     *
     * @return マスターTAGが存在する場合はtrue、それ以外はfalse
     */
    bool HasMaster() const;

    /**
     * @brief 自ノードが現在のマスターTAGか確認します。
     *
     * @return 自ノードがマスターの場合はtrue、それ以外はfalse
     */
    bool IsSelfMaster() const;

    /**
     * @brief 現在のマスターTAG識別情報を取得します。
     *
     * @return 現在のマスターTAG識別情報
     */
    const TagMasterIdentity& GetMaster() const;

    /**
     * @brief 未取得のマスター変更通知を1件取得します。
     *
     * @param change マスター変更通知の格納先
     * @return 通知を取得した場合はtrue、それ以外はfalse
     */
    bool TryTakeMasterChange(TagMasterChange& change);

private:
    /**
     * @brief 選出候補のノード状態と自ノード判定を表します。
     */
    struct MasterCandidate
    {
        NodeStatus status{};
        bool isSelf = false;
    };

    /**
     * @brief 現在有効でID重複のない最小TAGを選出します。
     *
     * @param nowMs 現在の単調増加ミリ秒時刻
     * @param candidate 選出候補の格納先
     * @return 候補が存在する場合はtrue、それ以外はfalse
     */
    bool SelectCandidate(
        uint32_t nowMs,
        MasterCandidate& candidate) const;

    /**
     * @brief 選出候補を現在のマスターへ反映します。
     *
     * @param candidate 選出した候補
     */
    void ApplyCandidate(const MasterCandidate& candidate);

    /**
     * @brief マスターが存在しない状態へ移行します。
     */
    void ClearMaster();

    /**
     * @brief 非0の新しいマスターセッションIDを生成します。
     *
     * @return 非0のマスターセッションID
     */
    uint32_t GenerateSessionId() const;

    EspNowBroadcast& m_broadcast;
    uint32_t m_startedAtMs = 0;
    bool m_started = false;
    bool m_electionComplete = false;
    bool m_isSelfMaster = false;
    bool m_hasPendingChange = false;
    TagMasterIdentity m_master{};
    TagMasterChange m_pendingChange{};
};

static_assert(
    TagMasterCoordinator::m_startupElectionWaitMs == 500,
    "Startup election wait must remain 500ms");
static_assert(
    TagMasterCoordinator::m_nodeExpirationMs == 30000,
    "Node expiration must remain 30 seconds");
