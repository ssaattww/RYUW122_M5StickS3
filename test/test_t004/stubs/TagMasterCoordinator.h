#pragma once

#include <array>
#include <cstdint>

/**
 * @brief NTP native test用のマスター識別情報を表します。
 */
struct TagMasterIdentity
{
    bool isValid = false;
    uint8_t nodeID = 0;
    std::array<uint8_t, 6> macAddress{};
    uint32_t sessionId = 0;
};

/**
 * @brief NTP native test用の選出済みマスター状態を提供します。
 */
class TagMasterCoordinator
{
public:
    static constexpr uint32_t m_nodeExpirationMs = 30000;

    /**
     * @brief 選出済みマスターを設定します。
     *
     * @param identity マスター識別情報
     * @param selfMaster 自ノードがマスターの場合はtrue
     */
    void SetMaster(
        const TagMasterIdentity& identity,
        bool selfMaster)
    {
        m_master = identity;
        m_selfMaster = selfMaster;
    }

    /**
     * @brief 有効なマスターが存在するか確認します。
     *
     * @return 有効な場合はtrue
     */
    bool HasMaster() const
    {
        return m_master.isValid && m_master.sessionId != 0;
    }

    /**
     * @brief 自ノードがマスターか確認します。
     *
     * @return 自ノードがマスターの場合はtrue
     */
    bool IsSelfMaster() const
    {
        return m_selfMaster;
    }

    /**
     * @brief 現在のマスター識別情報を取得します。
     *
     * @return マスター識別情報
     */
    const TagMasterIdentity& GetMaster() const
    {
        return m_master;
    }

private:
    TagMasterIdentity m_master{};
    bool m_selfMaster = false;
};
