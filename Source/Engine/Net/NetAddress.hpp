/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>

#include <Core/Utilities/Result.hpp>

#ifdef HYP_WINDOWS
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

namespace Hyperion {
namespace net {

struct NetAddress
{
    uint32 ipV4;
    uint16 port;

    NetAddress()
        : ipV4(0),
          port(0)
    {
    }

    NET_API explicit NetAddress(const sockaddr_in& addr);

    NET_API static TResult<NetAddress> TryResolve(const ANSIString& hostname, uint16 port);

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return ipV4 != 0 || port != 0;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    HYP_FORCE_INLINE constexpr bool operator==(const NetAddress& other) const
    {
        return ipV4 == other.ipV4
            && port == other.port;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const NetAddress& other) const
    {
        return ipV4 != other.ipV4
            || port != other.port;
    }
    
    NET_API sockaddr_in ToSockAddr() const;

    NET_API String ToString() const;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(ipV4)
            .Combine(port);
    }
};

} // namespace net

using net::NetAddress;

} // namespace Hyperion
