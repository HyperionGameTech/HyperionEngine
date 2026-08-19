/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 */

#include <Net/NetClient.hpp>

#include <cstdio>

namespace Hyperion {
namespace net {

static constexpr TimeDiff KeepAliveInterval = TimeDiff(2000);
static constexpr TimeDiff ServerTimeout = TimeDiff(15000);
static constexpr TimeDiff ConnectTimeout = TimeDiff(5000);

NetClient::NetClient()
    : m_reliableChannel(NetChannelMode::ReliableOrdered),
      m_unreliableChannel(NetChannelMode::UnreliableOrdered)
{
    m_dispatcher.RegisterHandler(NetMessageId::ConnectAccept,
        [this](const NetMessageContext&, ConstByteView)
        {
            if (GetConnectionState() == NetClientConnectionState::Connecting)
            {
                m_connectionState.Set(NetClientConnectionState::Connected, MemoryOrder::RELEASE);
            }
        });
}

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

    m_reliableChannel.Send(m_socket, m_serverAddress, NetMessage { NetMessageId::ConnectRequest, 0, ConstByteView() });

    return {};
}

void NetClient::Disconnect()
{
    m_connectionState.Set(NetClientConnectionState::Disconnected, MemoryOrder::RELEASE);

    m_socket.Close();
}

void NetClient::RegisterHandler(NetMessageId messageId, NetMessageHandler&& handler)
{
    m_dispatcher.RegisterHandler(messageId, std::move(handler));
}

void NetClient::Send(NetMessageId messageId, NetChannelMode mode, NetStreamKey key, ConstByteView payload)
{
    NetChannel& channel = IsReliable(mode) ? m_reliableChannel : m_unreliableChannel;

    channel.Send(m_socket, m_serverAddress, NetMessage { messageId, key, payload });
}

void NetClient::Update()
{
    const NetClientConnectionState state = GetConnectionState();

    if (state == NetClientConnectionState::Disconnected)
    {
        return;
    }

    m_reliableChannel.Update(m_socket, m_serverAddress);

    if (state == NetClientConnectionState::Connected
        && Time::Now() - m_lastKeepAliveTime >= KeepAliveInterval)
    {
        m_unreliableChannel.Send(m_socket, m_serverAddress, NetMessage { NetMessageId::KeepAlive, 0, ConstByteView() });

        m_lastKeepAliveTime = Time::Now();
    }

    NetAddress senderAddress;
    NetBuffer data;

    while (!m_socket.RecvFrom(senderAddress, data).HasError())
    {
        if (senderAddress != m_serverAddress)
        {
            continue;
        }

        m_lastActivityTime = Time::Now();

        m_dispatcher.Dispatch(m_socket, senderAddress, NetConnectionId(0),
            m_reliableChannel, m_unreliableChannel, data.ToByteView());
    }

    if (state == NetClientConnectionState::Connecting)
    {
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
