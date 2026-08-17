/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifdef HYP_WINDOWS
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   pragma comment(lib, "Ws2_32.lib")
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <errno.h>
#endif

#include <Core/Net/NetSocketUDP.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

namespace Hyperion {
namespace net {

NetSocketUDP::~NetSocketUDP()
{
    Close();
}

Result NetSocketUDP::Bind(uint16 port)
{
#ifdef HYP_WINDOWS
    static bool s_wsaInitialized = false;

    if (!s_wsaInitialized)
    {
        WSADATA wsaData;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            return HYP_MAKE_ERROR(Error, "WSAStartup failed");
        }

        s_wsaInitialized = true;
    }
#endif

    m_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (m_handle == InvalidHandle)
    {
        return HYP_MAKE_ERROR(Error, "Failed to create UDP socket");
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(m_handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        Close();

        return HYP_MAKE_ERROR(Error, "Failed to bind UDP socket to port {}", port);
    }

    // non-blocking
#ifdef HYP_WINDOWS
    u_long mode = 1;
    ioctlsocket(m_handle, FIONBIO, &mode);
#else
    int flags = fcntl(m_handle, F_GETFL, 0);
    fcntl(m_handle, F_SETFL, flags | O_NONBLOCK);
#endif

    return {}; // ok
}

void NetSocketUDP::Close()
{
    if (m_handle == InvalidHandle)
    {
        return;
    }

#ifdef HYP_WINDOWS
    closesocket(m_handle);
#else
    close(m_handle);
#endif

    m_handle = InvalidHandle;
}

Result NetSocketUDP::SendTo(const NetAddress& destination, ConstByteView data)
{
    if (!IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Socket is not valid");
    }

    sockaddr_in addr = destination.ToSockAddr();

    const int sent = sendto(
        m_handle,
        reinterpret_cast<const char*>(data.Data()),
        int(data.Size()),
        0,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));

    if (sent != int(data.Size()))
    {
        return HYP_MAKE_ERROR(Error, "Failed to send UDP packet : size mismatch ({} != {})", sent, data.Size());
    }

    return {};
}

Result NetSocketUDP::RecvFrom(NetAddress& outSender, Array<uint8, NetAllocator>& outData)
{
    if (!IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Socket is not valid");
    }

    constexpr size_t MaxDatagramSize = 1200;
    uint8 buffer[MaxDatagramSize] {};

    sockaddr_in senderAddr {};

#ifdef HYP_WINDOWS
    int addrLen = sizeof(senderAddr);
#else
    socklen_t addrLen = sizeof(senderAddr);
#endif

    const int received = recvfrom(
        m_handle,
        reinterpret_cast<char*>(buffer),
        int(MaxDatagramSize),
        0,
        reinterpret_cast<sockaddr*>(&senderAddr),
        &addrLen);

    if (received <= 0)
    {
        return HYP_MAKE_ERROR(Error, "Failed to receive UDP packet");
    }

    outSender = NetAddress(senderAddr);
    outData = Array<uint8, NetAllocator>(buffer, buffer + received);

    return {};
}

} // namespace net
} // namespace Hyperion
