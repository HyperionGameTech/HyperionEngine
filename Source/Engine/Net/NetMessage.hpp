/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <type_traits>

namespace Hyperion {
namespace net {

NET_API HYP_DECLARE_LOG_CHANNEL(Net);

enum class NetStreamKey : uint32;

// 2 bits
enum class NetChannelMode : uint8
{
    OrderedBit = 0b01,
    ReliableBit = 0b10,

    UnreliableUnordered = 0,
    UnreliableOrdered = OrderedBit,
    ReliableUnordered = ReliableBit,
    ReliableOrdered = ReliableBit | OrderedBit
};

static constexpr uint8 CurrentProtocolVersion = 1;

HYP_FORCE_INLINE constexpr bool IsReliable(NetChannelMode mode)
{
    // 0b10 == the ordered bit
    return (uint8(mode) & uint8(NetChannelMode::ReliableBit)) != 0;
}

HYP_FORCE_INLINE constexpr bool IsMoreRecent(uint32 a, uint32 b)
{
    return (a != b) && (uint32(a - b) < 0x80000000u);
}

enum class NetMessageId : uint16
{
    Invalid = 0,
    KeepAlive,
    ConnectRequest,
    ConnectAccept,
    Disconnect,
    Ack,
    EntitySpawn,
    EntityDespawn,
    EntityOwnershipChanged,
    ComponentSnapshot,
    
    ServerRequestsBegin,
    EntityTransformRequest = ServerRequestsBegin,
    PlayerInputRequest,
    // End Server Requests

    Reserved_GameStart = 1000
};

static constexpr inline bool IsServerRequest(NetMessageId id)
{
    return uint16(id) >= uint16(NetMessageId::ServerRequestsBegin)
        && uint16(id) < uint16(NetMessageId::Reserved_GameStart);
}

struct NetMessageHeader
{
    uint8 protocolVersion;
    NetChannelMode channelMode;
    NetMessageId messageId;
    NetStreamKey key;
    uint32 sequence;

    void Serialize(ByteWriter& stream) const
    {
        stream.Write(protocolVersion);
        stream.Write(channelMode);
        stream.Write(messageId);
        stream.Write(key);
        stream.Write(sequence);
    }

    void Deserialize(ByteReader& stream)
    {
        stream.Read(&protocolVersion);
        stream.Read(&channelMode);
        stream.Read(&messageId);
        stream.Read(&key);
        stream.Read(&sequence);
    }
};

static_assert(std::has_unique_object_representations_v<NetMessageHeader>);

struct NetMessage
{
    NetMessageId messageId;
    NetStreamKey key;
    ConstByteView payload; // owned elsewhere. take care to keep alive throughout the lifetime of the message!
};

} // namespace net
} // namespace Hyperion
