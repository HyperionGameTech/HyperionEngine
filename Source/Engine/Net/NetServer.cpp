/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
*/

// Needs to be before including NetServer.hpp
#include <Core/Memory/Pool/Pool.hpp>

#include <Net/NetServer.hpp>
#include <Net/NetChannel.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {
namespace net {

static constexpr TimeDiff ConnectionTimeout = TimeDiff(25000);

static constexpr uint32 MaxConnections = 64;

// Each connection spawns a player entity, so throttle how fast unknown addresses can open new ones
static constexpr uint32 MaxNewConnectionsPerWindow = 8;
static constexpr TimeDiff NewConnectionWindow = TimeDiff(1000);

// Clients only ever open a couple of streams to the server; stops a peer allocating state for every 32-bit key
static constexpr uint32 MaxIncomingStreamsPerChannel = 64;

#pragma region NetConnection

class NetConnection
{
public:
    NetConnection(NetConnectionId id, const NetAddress& address)
        : m_id(id),
          m_address(address),
          m_lastActivityTime(Time::Now()),
          m_reliableChannel(NetChannelMode::ReliableOrdered, MaxIncomingStreamsPerChannel),
          m_unreliableChannel(NetChannelMode::UnreliableOrdered, MaxIncomingStreamsPerChannel)
    {
    }

    ~NetConnection() = default;

    NetConnectionId GetId() const
    {
        return m_id;
    }

    const NetAddress& GetAddress() const
    {
        return m_address;
    }

    void UpdateActivity()
    {
        m_lastActivityTime = Time::Now();
    }

    TimeDiff TimeSinceLastActivity() const
    {
        return Time::Now() - m_lastActivityTime;
    }

    NetChannel& GetReliableChannel()
    {
        return m_reliableChannel;
    }

    NetChannel& GetUnreliableChannel()
    {
        return m_unreliableChannel;
    }

private:
    NetConnectionId m_id;
    NetAddress m_address;
    Time m_lastActivityTime;
    NetChannel m_reliableChannel;
    NetChannel m_unreliableChannel;
};

#pragma endregion NetConnection

#pragma region NetServer

static NetMessageHandler GetNoOpHandler()
{

    return [](const NetMessageContext&, ConstByteView)
    {
    };
}

NetServer::NetServer()
    : m_nextConnectionId(1),
      m_newConnectionWindowStart(Time::Now()),
      m_numNewConnectionsInWindow(0)
{
    m_dispatcher.RegisterHandler(NetMessageId::ConnectRequest, GetNoOpHandler());

    // Get Ping, send Pong
    m_dispatcher.RegisterHandler(NetMessageId::Ping,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            SendMessageTo(context.connectionId, NetMessageId::Pong,
                NetChannelMode::UnreliableOrdered, NetStreamKey(0), payload);
        });
}

NetServer::~NetServer()
{
    StopListening();
}

Result NetServer::Listen(uint16 port)
{
    if (m_socket.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Server is already listening");
    }

    return m_socket.Bind(port);
}

bool NetServer::IsListening() const
{
    return m_socket.IsValid();
}

void NetServer::StopListening()
{
    m_socket.Close();

    m_addrToConnectionId.Clear();
    m_connections.Clear();
}

void NetServer::RegisterHandler(NetMessageId messageId, NetMessageHandler&& handler)
{
    m_dispatcher.RegisterHandler(messageId, std::move(handler));
}

void NetServer::SendMessageTo(NetConnectionId connectionId, NetMessageId messageId, NetChannelMode mode, NetStreamKey key, ConstByteView payload)
{
    auto it = m_connections.Find(connectionId);

    if (it == m_connections.End())
    {
        return;
    }

    NetConnection& connection = *it->second;
    NetChannel& channel = IsReliable(mode) ? connection.GetReliableChannel() : connection.GetUnreliableChannel();

    channel.Send(m_socket, connection.GetAddress(), NetMessage { messageId, key, payload });
}

void NetServer::Broadcast(NetMessageId messageId, NetChannelMode mode, NetStreamKey key, ConstByteView payload)
{
    for (auto it = m_connections.Begin(); it != m_connections.End(); ++it)
    {
        NetConnection& connection = *it->second;
        NetChannel& channel = IsReliable(mode) ? connection.GetReliableChannel() : connection.GetUnreliableChannel();

        channel.Send(m_socket, connection.GetAddress(), NetMessage { messageId, key, payload });
    }
}

void NetServer::Update()
{
    if (!m_socket.IsValid())
    {
        return;
    }

    NetAddress senderAddress;
    NetBuffer data;

    while (!m_socket.RecvFrom(senderAddress, data).HasError())
    {
        const ConstByteView datagram(data.Data(), data.Size());

        NetConnection* connection = nullptr;
        bool isNewConnection = false;

        auto addrIt = m_addrToConnectionId.Find(senderAddress);

        if (addrIt == m_addrToConnectionId.End())
        {
            if (datagram.Size() < sizeof(NetMessageHeader))
            {
                continue;
            }

            MemoryByteReader reader(datagram);

            NetMessageHeader header {};
            header.Deserialize(reader);

            if (header.protocolVersion != CurrentProtocolVersion || header.messageId != NetMessageId::ConnectRequest)
            {
                continue;
            }

            if (m_connections.Size() >= MaxConnections)
            {
                HYP_LOG_ONCE(Net, Warning, "Rejecting connection from {}: server is full ({} connections)", senderAddress.ToString(), MaxConnections);

                continue;
            }

            if (!TryAcceptNewConnection())
            {
                continue;
            }

            const NetConnectionId connectionId = NetConnectionId(m_nextConnectionId++);

            auto insertResult = m_connections.Insert(connectionId, MakeUniqueWithAllocator<NetConnection, NetAllocator>(connectionId, senderAddress));
            m_addrToConnectionId.Insert(senderAddress, connectionId);

            connection = insertResult.first->second.Get();
            isNewConnection = true;

            OnClientConnected(NetServerConnectionStateChangedData { connectionId, senderAddress });
        }
        else
        {
            auto connectionIt = m_connections.Find(addrIt->second);

            if (connectionIt == m_connections.End())
            {
                continue;
            }

            connection = connectionIt->second.Get();
            connection->UpdateActivity();
        }

        m_dispatcher.Dispatch(m_socket, senderAddress, connection->GetId(),
            connection->GetReliableChannel(), connection->GetUnreliableChannel(), datagram);

        if (isNewConnection)
        {
            const uint32 connectionIdValue = uint32(connection->GetId());

            connection->GetReliableChannel().Send(
                m_socket, senderAddress,
                NetMessage { NetMessageId::ConnectAccept, NetStreamKey(0),
                    ConstByteView(reinterpret_cast<const ubyte*>(&connectionIdValue), sizeof(connectionIdValue)) });
        }
    }

    for (auto it = m_connections.Begin(); it != m_connections.End(); ++it)
    {
        it->second->GetReliableChannel().Update(m_socket, it->second->GetAddress());
    }

    for (auto it = m_connections.Begin(); it != m_connections.End();)
    {
        if (it->second->TimeSinceLastActivity() >= ConnectionTimeout)
        {
            const NetConnectionId connectionId = it->second->GetId();
            const NetAddress address = it->second->GetAddress();

            m_addrToConnectionId.Erase(address);
            it = m_connections.Erase(it);

            OnClientDisconnected(NetServerConnectionStateChangedData { connectionId, address });

            continue;
        }

        ++it;
    }
}

bool NetServer::TryAcceptNewConnection()
{
    const Time now = Time::Now();

    if (now - m_newConnectionWindowStart >= NewConnectionWindow)
    {
        m_newConnectionWindowStart = now;
        m_numNewConnectionsInWindow = 0;
    }

    if (m_numNewConnectionsInWindow >= MaxNewConnectionsPerWindow)
    {
        return false;
    }

    m_numNewConnectionsInWindow++;

    return true;
}

#pragma endregion NetServer

} // namespace net
} // namespace Hyperion
