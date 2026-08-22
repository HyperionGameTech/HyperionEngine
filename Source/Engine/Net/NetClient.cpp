/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 */

#include <Net/NetClient.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <cstdio>

namespace Hyperion {
namespace net {

static constexpr TimeDiff KeepAliveInterval = TimeDiff(2000);
static constexpr TimeDiff ServerTimeout = TimeDiff(20000);
static constexpr TimeDiff ConnectTimeout = TimeDiff(10000);

NetClient::NetClient()
    : m_connectionId(Invalid<NetConnectionId>),
      m_rttMilliseconds(0),
      m_reliableChannel(NetChannelMode::ReliableOrdered),
      m_unreliableChannel(NetChannelMode::UnreliableOrdered)
{
    m_dispatcher.RegisterHandler(NetMessageId::ConnectAccept,
        [this](const NetMessageContext&, ConstByteView payload)
        {
            if (GetConnectionState() == NetClientConnectionState::Connecting)
            {
                if (payload.Size() >= sizeof(uint32))
                {
                    uint32 connectionIdValue = 0;

                    MemoryByteReader reader { payload };
                    reader.Read(&connectionIdValue, sizeof(uint32));

                    m_connectionId = NetConnectionId(connectionIdValue);
                }

                m_connectionState.Set(NetClientConnectionState::Connected, MemoryOrder::RELEASE);

                OnConnected(NetClientConnectionStateChangedData { m_serverAddress });
            }
        });

    // Keep-alive doubles as a ping: the server echoes our send timestamp back to us,
    // letting us measure round-trip time.
    m_dispatcher.RegisterHandler(NetMessageId::KeepAlive,
        [this](const NetMessageContext&, ConstByteView payload)
        {
            if (payload.Size() >= sizeof(uint64))
            {
                uint64 pingSendTimeMs = 0;

                MemoryByteReader reader { payload };
                reader.Read(&pingSendTimeMs, sizeof(uint64));

                const int64 rttMs = int64(Time::Now().ToMilliseconds()) - int64(pingSendTimeMs);

                if (rttMs >= 0)
                {
                    m_rttMilliseconds.Set(rttMs, MemoryOrder::RELEASE);
                }
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

    m_reliableChannel.Send(
        m_socket,
        m_serverAddress,
        NetMessage { NetMessageId::ConnectRequest, NetStreamKey(0), ConstByteView() });

    return {};
}

void NetClient::Disconnect()
{
    const NetClientConnectionState currState = m_connectionState.Get(MemoryOrder::ACQUIRE);

    const bool wasConnectedOrConnecting = (currState == NetClientConnectionState::Connecting
        || currState == NetClientConnectionState::Connected);

    m_connectionState.Set(NetClientConnectionState::Disconnected, MemoryOrder::RELEASE);
    m_socket.Close();

    if (wasConnectedOrConnecting)
    {
        OnDisconnected(NetClientConnectionStateChangedData { m_serverAddress });
    }
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
        NetBuffer pingPayload;
        MemoryByteWriter<NetAllocator, 1> pingWriter(&pingPayload);

        const Time pingSendTime = Time::Now();
        pingWriter.Write(pingSendTime.ToMilliseconds());

        m_unreliableChannel.Send(
            m_socket,
            m_serverAddress,
            NetMessage { NetMessageId::KeepAlive, NetStreamKey(0), pingPayload.ToByteView() });

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

        m_dispatcher.Dispatch(
            m_socket,
            senderAddress,
            Invalid<NetConnectionId>,
            m_reliableChannel,
            m_unreliableChannel,
            data.ToByteView());
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
        Disconnect();
    }
}

} // namespace net
} // namespace Hyperion
