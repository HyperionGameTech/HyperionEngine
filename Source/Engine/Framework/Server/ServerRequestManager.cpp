/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Framework/Server/ServerRequestManager.hpp>

#include <Net/NetServer.hpp>

#include <Core/IO/ByteReader.hpp>

namespace Hyperion {

using net::NetAllocator;
using net::NetMessageContext;
using net::NetMessageId;

void ServerRequestManager::RegisterHandlers(net::NetServer& netServer)
{
    netServer.RegisterHandler(NetMessageId::EntityTransformRequest,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            MemoryByteReader reader { payload };

            Vec3f translation;
            Quat4f rotation;
            Vec3f scale;

            reader.Read(&translation, sizeof(Vec3f));
            reader.Read(&rotation, sizeof(Quat4f));
            reader.Read(&scale, sizeof(Vec3f));

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
