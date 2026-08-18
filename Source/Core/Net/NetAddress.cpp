/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Net/NetAddress.hpp>

#if defined(HYP_UNIX) || defined(HYP_ANDROID)
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#elif defined(HYP_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace Hyperion {
namespace net {

static bool ResolveHost(const ANSIString& host, uint16 port, sockaddr_in& outAddr)
{
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* results = nullptr;

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", port);

    if (getaddrinfo(host.Data(), portStr, &hints, &results) != 0 || results == nullptr)
    {
        return false;
    }

    outAddr = *reinterpret_cast<sockaddr_in*>(results->ai_addr);

    freeaddrinfo(results);

    return true;
}

NetAddress::NetAddress(const sockaddr_in &addr)
    : ipV4(addr.sin_addr.s_addr),
      port(ntohs(addr.sin_port))
{
}

TResult<NetAddress> NetAddress::TryResolve(const ANSIString& hostname, uint16 port)
{
    ANSIString hostPart = hostname;

    const size_t colonIndex = hostname.FindLastIndex(':');

    if (colonIndex != ANSIString::NotFound)
    {
        uint16 parsedPort = 0;
        bool validPort = colonIndex + 1 < hostname.Length();

        for (size_t i = colonIndex + 1; validPort && i < hostname.Length(); i++)
        {
            const char c = hostname.Data()[i];

            if (c < '0' || c > '9')
            {
                validPort = false;

                break;
            }

            parsedPort = uint16(parsedPort * 10 + (c - '0'));
        }

        if (validPort)
        {
            hostPart = ANSIString(hostname.Substr(0, colonIndex));
            port = parsedPort;
        }
    }

    struct sockaddr_in addr {};

    if (!ResolveHost(hostPart, port, addr))
    {
        return HYP_MAKE_ERROR(Error, "Failed to resolve host: {}", hostPart);
    }

    return NetAddress(addr);
}

sockaddr_in NetAddress::ToSockAddr() const
{
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ipV4;

    return addr;
}

String NetAddress::ToString() const
{
    char buffer[INET_ADDRSTRLEN] {};
    inet_ntop(AF_INET, &ipV4, buffer, sizeof(buffer));

    return String(buffer) + ":" + String::ToString(port);
}

} // namespace net
} // namespace Hyperion
