#pragma once

#include <cstdint>

class EspNowTransport;

/**
 * @brief 既知consumer処理後のESP-NOW受信FIFOへ最終所有境界を提供します。
 */
class EspNowReceiveQueueTerminator
{
public:
    /**
     * @brief 最終所有境界で使用する共有transportを設定します。
     *
     * @param transport 既知consumerと共有するESP-NOW transport
     */
    explicit EspNowReceiveQueueTerminator(EspNowTransport& transport);

    /**
     * @brief transport更新後かつ既知consumer処理前のFIFO状態を記録します。
     * callback enqueueとのcycle境界はEspNowTransport::Update()直後に固定し、
     * cycle開始後に到着したpacketはterminal処理を次cycleへ延期します。
     */
    void BeginCycle();

    /**
     * @brief 既知consumerが進めなかったFIFO先頭を最大1件だけ破棄します。
     * cycle内で削除件数が変化した場合は破棄を次cycleへ延期します。
     */
    void Update();

    /**
     * @brief 最終所有境界で破棄したpacket件数を取得します。
     *
     * @return 破棄済みpacket件数
     */
    uint32_t GetDiscardedPacketCount() const;

private:
    EspNowTransport& m_transport;
    uint32_t m_consumedReceiveCountAtCycleStart = 0;
    uint32_t m_discardedPacketCount = 0;
    bool m_hadPacketAtCycleStart = false;
    bool m_cycleStarted = false;
};
