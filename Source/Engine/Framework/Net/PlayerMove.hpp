/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion {

// Maximum number of moves that fits in a single PlayerMovesRequest datagram.
static constexpr uint32 MaxPlayerMovesPerRequest = 16;

// One simulation tick of player movement input. Sent client -> server in batches
// (PlayerMovesRequest) and replayed locally for client-side prediction.
struct PlayerMove
{
    uint32 moveId = 0;
    float deltaTime = 0.0f; // seconds of simulation this move covers
    Vec2f movementInput = Vec2f(0.0f);
    int8 jumpRequested = 0;
    Vec3f viewDirection = Vec3f(0.0f, 0.0f, 1.0f);
};

static_assert(std::is_trivially_copyable_v<PlayerMove>);
static_assert(std::is_trivially_destructible_v<PlayerMove>);

// Server -> client acknowledgement of processed moves, carrying the authoritative
// entity translation as of the last acked move. The client compares this against
// its own predicted translation for that move and corrects itself if needed.
struct PlayerMoveAck
{
    uint32 ackedMoveId = 0;
    Vec3f authoritativeTranslation = Vec3f(0.0f);
};

static_assert(std::is_trivially_copyable_v<PlayerMoveAck>);
static_assert(std::is_trivially_destructible_v<PlayerMoveAck>);

HYP_FORCE_INLINE void SerializePlayerMoves(ByteWriter& writer, uint32 lastAckedMoveId, const PlayerMove* moves, uint32 numMoves)
{
    writer.Write(lastAckedMoveId);
    writer.Write(uint8(numMoves));

    for (uint32 i = 0; i < numMoves; ++i)
    {
        writer.Write(moves[i].moveId);
        writer.Write(moves[i].deltaTime);
        writer.Write(moves[i].movementInput);
        writer.Write(moves[i].jumpRequested);
        writer.Write(moves[i].viewDirection);
    }
}

// Deserializes a batch of moves. Returns the number of moves read (clamped to maxMoves).
HYP_FORCE_INLINE uint32 DeserializePlayerMoves(ByteReader& reader, uint32& outLastAckedMoveId, PlayerMove* outMoves, uint32 maxMoves)
{
    uint8 numMoves = 0;

    reader.Read(&outLastAckedMoveId, sizeof(uint32));
    reader.Read(&numMoves, sizeof(uint8));

    const uint32 count = MathUtil::Min(uint32(numMoves), maxMoves);

    for (uint32 i = 0; i < count; ++i)
    {
        reader.Read(&outMoves[i].moveId, sizeof(uint32));
        reader.Read(&outMoves[i].deltaTime, sizeof(float));
        reader.Read(&outMoves[i].movementInput, sizeof(Vec2f));
        reader.Read(&outMoves[i].jumpRequested, sizeof(int8));
        reader.Read(&outMoves[i].viewDirection, sizeof(Vec3f));
    }

    return count;
}

HYP_FORCE_INLINE void SerializePlayerMoveAck(ByteWriter& writer, const PlayerMoveAck& ack)
{
    writer.Write(ack.ackedMoveId);
    writer.Write(ack.authoritativeTranslation);
}

HYP_FORCE_INLINE PlayerMoveAck DeserializePlayerMoveAck(ByteReader& reader)
{
    PlayerMoveAck ack;

    reader.Read(&ack.ackedMoveId, sizeof(uint32));
    reader.Read(&ack.authoritativeTranslation, sizeof(Vec3f));

    return ack;
}

} // namespace Hyperion
