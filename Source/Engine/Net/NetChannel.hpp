/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/Pimpl.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Utilities/Time.hpp>
#include <Core/Utilities/ValueStorage.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMemory.hpp>
#include <Net/NetSocketUDP.hpp>
#include <Net/NetAddress.hpp>

#include <type_traits>

namespace Hyperion {
namespace net {

struct StreamState;
struct StreamStateMap;

class NET_API NetChannel final
{
public:
    explicit NetChannel(NetChannelMode mode);

    NetChannel(const NetChannel&) = delete;
    NetChannel& operator=(const NetChannel&) = delete;

    ~NetChannel();

    HYP_FORCE_INLINE NetChannelMode GetMode() const
    {
        return m_mode;
    }

    void Send(NetSocketUDP& socket, const NetAddress& destAddr, const NetMessage& message);

    void HandleIncoming(
        NetSocketUDP& socket,
        const NetAddress& srcAddr,
        const NetMessageHeader& header,
        ConstByteView payload,
        const ProcRef<void(NetMessageId, ConstByteView)>& dispatch);

    void OnAck(NetStreamKey key, uint32 sequence);

    void Update(NetSocketUDP& socket, const NetAddress& destAddr);

private:
    StreamState& GetOrCreateStream(NetStreamKey key);

    void SendAck(NetSocketUDP& socket, const NetAddress& destAddr, NetStreamKey key, uint32 sequence);

    const NetChannelMode m_mode;
    Pimpl<StreamStateMap> m_streams;

    // Temp memory stream used for writing the unreliable payloads.
    NetBuffer m_tempBuffer;
};

} // namespace net
} // namespace Hyperion
