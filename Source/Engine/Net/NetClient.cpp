/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Net/NetClient.hpp>

#include <cstdio>

namespace Hyperion {
namespace net {

static constexpr TimeDiff KeepAliveInterval = TimeDiff(2000);
static constexpr TimeDiff ServerTimeout = TimeDiff(15000);
static constexpr TimeDiff ConnectTimeout = TimeDiff(5000);

NetClient::NetClient() = default;

NetClient::~NetClient()
{
    Disconnect();
}

Result NetClient::Connect(const NetAddress& serverAddress)
{
    if (GetConnectionState() != NetClientConnectionState::Disconnected)
    {
        return HYP_MAKE_ERROR(Error, "Already connected or connecting");
    }

    if (Result bindResult = m_socket.Bind(0); bindResult.HasError())
    {
        return bindResult;
    }

    m_serverAddress = serverAddress;
    m_lastActivityTime = Time::Now();
    m_lastKeepAliveTime = Time(0);
    m_connectStartTime = Time::Now();

    m_connectionState.Set(NetClientConnectionState::Connecting, MemoryOrder::RELEASE);

    return {};
}

void NetClient::Disconnect()
{
    m_connectionState.Set(NetClientConnectionState::Disconnected, MemoryOrder::RELEASE);

    m_socket.Close();
}

void NetClient::Update()
{
    const NetClientConnectionState state = GetConnectionState();

    if (state == NetClientConnectionState::Disconnected)
    {
        return;
    }

    if (Time::Now() - m_lastKeepAliveTime >= KeepAliveInterval)
    {
        const uint8 keepAlive = 0;

        m_socket.SendTo(m_serverAddress, ConstByteView(&keepAlive, sizeof(keepAlive)));

        m_lastKeepAliveTime = Time::Now();
    }

    bool receivedFromServer = false;

    NetAddress senderAddress;
    Array<uint8, NetAllocator> data;

    while (!m_socket.RecvFrom(senderAddress, data).HasError())
    {
        if (senderAddress == m_serverAddress)
        {
            m_lastActivityTime = Time::Now();
            receivedFromServer = true;
        }
    }

    if (state == NetClientConnectionState::Connecting)
    {
        if (receivedFromServer)
        {
            m_connectionState.Set(NetClientConnectionState::Connected, MemoryOrder::RELEASE);

            return;
        }

        if (Time::Now() - m_connectStartTime >= ConnectTimeout)
        {
            {
                Mutex::Guard guard(m_lastErrorMutex);

                m_lastError = HYP_MAKE_ERROR(Error, "Timed out waiting for a response from {}", m_serverAddress.ToString());
            }

            Disconnect();
        }

        return;
    }

    // CONNECTED
    if (Time::Now() - m_lastActivityTime >= ServerTimeout)
    {
        const NetAddress serverAddress = m_serverAddress;

        Disconnect();

        OnDisconnected(NetServerDisconnectedData { serverAddress });
    }
}

} // namespace net
} // namespace Hyperion
