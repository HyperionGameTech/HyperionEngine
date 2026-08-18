/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Net/NetClient.hpp>

#include <cstdio>

namespace Hyperion {
namespace net {

static constexpr TimeDiff KeepAliveInterval = TimeDiff(2000);
static constexpr TimeDiff ServerTimeout = TimeDiff(15000);

NetClient::NetClient()
    : m_isConnected(false)
{
}

NetClient::~NetClient()
{
    Disconnect();
}

Result NetClient::Connect(const NetAddress& serverAddress)
{
    if (m_isConnected)
    {
        return HYP_MAKE_ERROR(Error, "Already connected");
    }

    if (Result bindResult = m_socket.Bind(0); bindResult.HasError())
    {
        return bindResult;
    }

    m_serverAddress = serverAddress;
    m_isConnected = true;
    m_lastActivityTime = Time::Now();
    m_lastKeepAliveTime = Time(0);

    return {};
}

void NetClient::Disconnect()
{
    m_isConnected = false;

    m_socket.Close();
}

void NetClient::Update()
{
    if (!m_isConnected)
    {
        return;
    }

    if (Time::Now() - m_lastKeepAliveTime >= KeepAliveInterval)
    {
        const uint8 keepAlive = 0;

        m_socket.SendTo(m_serverAddress, ConstByteView(&keepAlive, sizeof(keepAlive)));

        m_lastKeepAliveTime = Time::Now();
    }

    NetAddress senderAddress;
    Array<uint8, NetAllocator> data;

    while (!m_socket.RecvFrom(senderAddress, data).HasError())
    {
        if (senderAddress == m_serverAddress)
        {
            m_lastActivityTime = Time::Now();
        }
    }

    if (Time::Now() - m_lastActivityTime >= ServerTimeout)
    {
        const NetAddress serverAddress = m_serverAddress;

        Disconnect();

        OnDisconnected(NetServerDisconnectedData { serverAddress });
    }
}

} // namespace net
} // namespace Hyperion
