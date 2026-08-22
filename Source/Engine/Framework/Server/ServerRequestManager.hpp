/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Transform.hpp>
#include <Core/Math/Vector2.hpp>

#include <Core/Utilities/Traits.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMessageDispatcher.hpp>

#include <Framework/Net/ReplicationQueue.hpp>
#include <Framework/Net/PlayerMove.hpp>

namespace Hyperion {

namespace net {
class NetServer;
enum class NetConnectionId : uint32;
} // namespace net

enum class NetId : uint32;

enum class ServerRequestType : uint8
{
    TransformEntity,
    PlayerMoves
};

// ALL derived types must be trivially destructible, will be allocated using arena/transient allocators
//  with NO destructor call.
template <ServerRequestType>
struct ServerRequest;

struct ServerRequestBase
{
    ServerRequestType type;
    net::NetConnectionId connectionId;
    NetId netId;

    ServerRequestBase(ServerRequestType type, net::NetConnectionId connectionId, NetId netId)
        : type(type),
          connectionId(connectionId),
          netId(netId)
    {
    }
};

template <>
struct ServerRequest<ServerRequestType::TransformEntity> final : ServerRequestBase
{
    Transform transform;

    ServerRequest(net::NetConnectionId connectionId, NetId netId, const Transform& transform)
        : ServerRequestBase(ServerRequestType::TransformEntity, connectionId, netId),
          transform(transform)
    {
    }
};

template <>
struct ServerRequest<ServerRequestType::PlayerMoves> final : ServerRequestBase
{
    uint32 numMoves = 0;
    uint32 lastAckedMoveId = 0;
    PlayerMove moves[MaxPlayerMovesPerRequest];

    ServerRequest(net::NetConnectionId connectionId, uint32 lastAckedMoveId, const PlayerMove* inMoves, uint32 inNumMoves)
        : ServerRequestBase(ServerRequestType::PlayerMoves, connectionId, Invalid<NetId>),
          numMoves(inNumMoves),
          lastAckedMoveId(lastAckedMoveId)
    {
        for (uint32 i = 0; i < inNumMoves; ++i)
        {
            moves[i] = inMoves[i];
        }
    }
};

// Must all be trivial as they won't be destructed!
static_assert(std::is_trivially_destructible_v<ServerRequest<ServerRequestType::TransformEntity>>);
static_assert(std::is_trivially_destructible_v<ServerRequest<ServerRequestType::PlayerMoves>>);

class ENGINE_API ServerRequestManager
{
public:
    void RegisterHandlers(net::NetServer& netServer);

    void PublishBatch()
    {
        m_queue.PublishBatch();
    }

    template <class AllocatorType>
    void DrainPendingRequests(Array<ServerRequestBase*, AllocatorType>& outRequests)
    {
        m_queue.DrainPending(outRequests);
    }

private:
    template <ServerRequestType Type>
    void PushRequest(const ServerRequest<Type>& request)
    {
        m_queue.Push(request);
    }

    ReplicationQueue<ServerRequestBase> m_queue;
};

} // namespace Hyperion
