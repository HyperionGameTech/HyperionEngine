/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

// Needs to be before including NetServer.hpp
#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Net/NetServer.hpp>

namespace Hyperion {
namespace net {

#pragma region NetConnection

class NetConnection
{
public:
    NetConnection() = default;
    ~NetConnection() = default;
};

#pragma endregion NetConnection

#pragma region NetServer

NetServer::NetServer() = default;

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
}

#pragma endregion NetServer

} // namespace net
} // namespace Hyperion
