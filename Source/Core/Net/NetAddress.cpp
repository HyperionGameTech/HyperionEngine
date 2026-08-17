/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Net/NetAddress.hpp>

namespace Hyperion {
namespace net {

NetAddress::NetAddress(const sockaddr_in &addr)
    : ipV4(addr.sin_addr.s_addr),
      port(ntohs(addr.sin_port))
{
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
