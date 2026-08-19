/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Transform.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMessageDispatcher.hpp>

#include <Framework/Net/ReplicationQueue.hpp>

namespace Hyperion {

namespace net {
class NetServer;
enum class NetConnectionId : uint32;
} // namespace net

enum class NetId : uint32;

enum class ServerRequestType : uint8
{
    TransformEntity
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

// Must all be trivial as they won't be destructed!
static_assert(std::is_trivially_destructible_v<ServerRequest<ServerRequestType::TransformEntity>>);

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
