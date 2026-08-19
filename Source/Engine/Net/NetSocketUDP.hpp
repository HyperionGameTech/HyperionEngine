/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Net/NetMemory.hpp>
#include <Net/NetAddress.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Span.hpp>

namespace Hyperion {
namespace net {

class NET_API NetSocketUDP
{
public:
    NetSocketUDP() = default;

    NetSocketUDP(const NetSocketUDP& other) = delete;
    NetSocketUDP& operator=(const NetSocketUDP& other) = delete;

    ~NetSocketUDP();

    Result Bind(uint16 port);
    void Close();

    Result SendTo(const NetAddress& destination, ConstByteView data);
    Result RecvFrom(NetAddress& outSender, NetBuffer& outData);

    bool IsValid() const
        { return m_handle != InvalidHandle; }

private:
#ifdef HYP_WINDOWS
    using SocketHandle = uintptr_t;
    static constexpr SocketHandle InvalidHandle = ~0ull;
#else
    using SocketHandle = int;
    static constexpr SocketHandle InvalidHandle = -1;
#endif

    SocketHandle m_handle = InvalidHandle;
};

} // namespace net

using net::NetSocketUDP;

} // namespace Hyperion
