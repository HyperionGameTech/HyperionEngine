/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

// Needs to be before including NetServer.hpp
#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Net/NetServer.hpp>

#include <Core/Utilities/Time.hpp>

namespace Hyperion {
namespace net {

static constexpr TimeDiff ConnectionTimeout = TimeDiff(10000);

#pragma region NetConnection

class NetConnection
{
public:
    NetConnection(NetConnectionId id, const NetAddress& address)
        : m_id(id),
          m_address(address),
          m_lastActivityTime(Time::Now())
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

private:
    NetConnectionId m_id;
    NetAddress m_address;
    Time m_lastActivityTime;
};

#pragma endregion NetConnection

#pragma region NetServer

NetServer::NetServer()
    : m_nextConnectionId(1)
{
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

void NetServer::Update()
{
    if (!m_socket.IsValid())
    {
        return;
    }

    NetAddress senderAddress;
    Array<uint8, NetAllocator> data;

    while (!m_socket.RecvFrom(senderAddress, data).HasError())
    {
        auto addrIt = m_addrToConnectionId.Find(senderAddress);

        if (addrIt == m_addrToConnectionId.End())
        {
            const NetConnectionId connectionId = NetConnectionId(m_nextConnectionId++);

            m_addrToConnectionId.Insert(senderAddress, connectionId);
            m_connections.Insert(connectionId, MakeUniqueWithAllocator<NetConnection, NetAllocator>(connectionId, senderAddress));

            OnClientConnected(NetClientConnectedData { connectionId, senderAddress });

            continue;
        }

        auto connectionIt = m_connections.Find(addrIt->second);

        if (connectionIt != m_connections.End())
        {
            connectionIt->second->UpdateActivity();
        }
    }

    for (auto it = m_connections.Begin(); it != m_connections.End();)
    {
        if (it->second->TimeSinceLastActivity() >= ConnectionTimeout)
        {
            const NetConnectionId connectionId = it->second->GetId();
            const NetAddress address = it->second->GetAddress();

            m_addrToConnectionId.Erase(address);
            it = m_connections.Erase(it);

            OnClientDisconnected(NetClientDisconnectedData { connectionId, address });

            continue;
        }

        ++it;
    }
}

#pragma endregion NetServer

} // namespace net
} // namespace Hyperion
