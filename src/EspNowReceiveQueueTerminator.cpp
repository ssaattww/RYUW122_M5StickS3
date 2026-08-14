#include "EspNowReceiveQueueTerminator.h"

#include "EspNowTransport.h"

EspNowReceiveQueueTerminator::EspNowReceiveQueueTerminator(
    EspNowTransport& transport)
    : m_transport(transport)
{
}

void EspNowReceiveQueueTerminator::BeginCycle()
{
    m_consumedReceiveCountAtCycleStart =
        m_transport.GetConsumedReceiveCount();
    EspNowReceivedPacket packet{};
    m_hadPacketAtCycleStart = m_transport.PeekReceive(packet);
    m_cycleStarted = true;
}

void EspNowReceiveQueueTerminator::Update()
{
    if (!m_cycleStarted)
    {
        return;
    }
    m_cycleStarted = false;
    if (!m_hadPacketAtCycleStart ||
        m_transport.GetConsumedReceiveCount() !=
        m_consumedReceiveCountAtCycleStart)
    {
        return;
    }
    if (m_transport.ConsumeReceive())
    {
        ++m_discardedPacketCount;
    }
}

uint32_t EspNowReceiveQueueTerminator::GetDiscardedPacketCount() const
{
    return m_discardedPacketCount;
}
