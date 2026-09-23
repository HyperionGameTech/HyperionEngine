/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Framework/Server/ServerRequestManager.hpp>

#include <Net/NetServer.hpp>

#include <Core/IO/ByteReader.hpp>

#include <cmath>

namespace Hyperion {

using net::NetAllocator;
using net::NetMessageContext;
using net::NetMessageId;

static constexpr float MinRequestedScale = 1e-4f;
static constexpr float MaxRequestedScale = 1e4f;

static bool IsRequestedTransformValid(const Vec3f& translation, const Quat4f& rotation, const Vec3f& scale)
{
    for (float value : { translation.x, translation.y, translation.z, rotation.x, rotation.y, rotation.z, rotation.w })
    {
        if (!std::isfinite(value))
        {
            return false;
        }
    }

    for (float value : { scale.x, scale.y, scale.z })
    {
        if (!std::isfinite(value) || std::fabs(value) < MinRequestedScale || std::fabs(value) > MaxRequestedScale)
        {
            return false;
        }
    }

    const float rotationLengthSquared = rotation.LengthSquared();

    return rotationLengthSquared > 0.5f && rotationLengthSquared < 1.5f;
}

void ServerRequestManager::RegisterHandlers(net::NetServer& netServer)
{
    netServer.RegisterHandler(NetMessageId::EntityTransformRequest,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            if (payload.Size() < sizeof(Vec3f) + sizeof(Quat4f) + sizeof(Vec3f))
            {
                return;
            }

            MemoryByteReader reader { payload };

            Vec3f translation;
            Quat4f rotation;
            Vec3f scale;

            reader.Read(&translation, sizeof(Vec3f));
            reader.Read(&rotation, sizeof(Quat4f));
            reader.Read(&scale, sizeof(Vec3f));

            if (!IsRequestedTransformValid(translation, rotation, scale))
            {
                return;
            }

            rotation.Normalize();

            // ownership of the NetId is checked on the sim thread (ReplicationSystem::ApplyPendingRequests)
            PushRequest(ServerRequest<ServerRequestType::TransformEntity>(
                context.connectionId,
                NetId(uint32(context.key)),
                Transform(translation, scale, rotation)));
        });

    netServer.RegisterHandler(NetMessageId::PlayerMovesRequest,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            MemoryByteReader reader { payload };

            uint32 lastAckedMoveId = 0;
            PlayerMove moves[MaxPlayerMovesPerRequest];

            const uint32 numMoves = DeserializePlayerMoves(reader, lastAckedMoveId, moves, MaxPlayerMovesPerRequest);

            if (numMoves == 0)
            {
                return;
            }

            PushRequest(ServerRequest<ServerRequestType::PlayerMoves>(
                context.connectionId,
                lastAckedMoveId,
                moves,
                numMoves));
        });
}

} // namespace Hyperion
