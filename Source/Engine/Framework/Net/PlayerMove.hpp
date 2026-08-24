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

struct PlayerMove
{
    uint32 moveId;
    float deltaTime; // seconds of simulation this move covers
    float movementInput[2];
    float viewDirection[3];
    int8 jumpRequested;

    int8 _pad[3];

    PlayerMove() = default;

    HYP_FORCE_INLINE Vec2f GetMovementInput() const
    {
        return { movementInput[0], movementInput[1] };
    }

    HYP_FORCE_INLINE Vec3f GetViewDirection() const
    {
        return { viewDirection[0], viewDirection[1], viewDirection[2] };
    }
};

static_assert(std::is_trivially_copyable_v<PlayerMove>);
static_assert(std::is_trivially_destructible_v<PlayerMove>);

struct PlayerMoveAck
{
    float authTranslation[3];
    uint32 ackedMoveId;

    PlayerMoveAck() = default;

    PlayerMoveAck(const Vec3f& authTranslation, uint32 ackedMoveId)
        : authTranslation{ authTranslation.x, authTranslation.y, authTranslation.z },
          ackedMoveId(ackedMoveId)
    {
    }

    HYP_FORCE_INLINE Vec3f GetAuthTranslation() const
    {
        return Vec3f(authTranslation[0], authTranslation[1], authTranslation[2]);
    }
};

static_assert(std::is_trivially_copyable_v<PlayerMoveAck>);
static_assert(std::is_trivially_destructible_v<PlayerMoveAck>);

HYP_FORCE_INLINE void SerializePlayerMoves(ByteWriter& writer, uint32 lastAckedMoveId, const PlayerMove* moves, uint32 numMoves)
{
    writer.Write(lastAckedMoveId);
    writer.Write(uint8(numMoves));
    writer.Write(moves, sizeof(PlayerMove) * numMoves);
}

// Deserializes a batch of moves. Returns the number of moves read (clamped to maxMoves).
HYP_FORCE_INLINE uint32 DeserializePlayerMoves(ByteReader& reader, uint32& outLastAckedMoveId, PlayerMove* outMoves, uint32 maxMoves)
{
    uint8 numMoves = 0;

    reader.Read(&outLastAckedMoveId, sizeof(uint32));
    reader.Read(&numMoves, sizeof(uint8));

    const uint32 count = MathUtil::Min(uint32(numMoves), maxMoves);
    reader.Read(outMoves, sizeof(PlayerMove) * count);

    return count;
}

HYP_FORCE_INLINE void SerializePlayerMoveAck(ByteWriter& writer, const PlayerMoveAck& ack)
{
    writer.Write(&ack, sizeof(ack));
}

HYP_FORCE_INLINE PlayerMoveAck DeserializePlayerMoveAck(ByteReader& reader)
{
    PlayerMoveAck ack;
    reader.Read(&ack, sizeof(ack));

    return ack;
}

} // namespace Hyperion
