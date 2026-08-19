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

#include <type_traits>

namespace Hyperion {
namespace net {

using NetStreamKey = uint16;

// 2 bits
enum class NetChannelMode : uint8
{
    UnreliableUnordered = 0b00,
    UnreliableOrdered = 0b01,
    ReliableUnordered = 0b10,
    ReliableOrdered = 0b11
};

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
    Reserved_GameStart = 1000
};

struct NetMessageHeader
{
    uint8 protocolVersion;
    uint8 channelMode;
    uint16 messageId;
    uint32 sequence;
    NetStreamKey key;

    void Serialize(ByteWriter& stream) const
    {
        stream.Write(protocolVersion);
        stream.Write(channelMode);
        stream.Write(messageId);
        stream.Write(sequence);
        stream.Write(key);
    }

    void Deserialize(ByteReader& stream)
    {
        stream.Read(&protocolVersion);
        stream.Read(&channelMode);
        stream.Read(&messageId);
        stream.Read(&sequence);
        stream.Read(&key);
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
