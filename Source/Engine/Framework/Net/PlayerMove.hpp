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

#include <cmath>
#include <type_traits>

namespace Hyperion {

// Maximum number of moves that fits in a single PlayerMovesRequest datagram.
static constexpr uint32 MaxPlayerMovesPerRequest = 16;

// Longest step a single move may simulate, matches the character controller's substep clamp (3 x 1/60).
static constexpr float MaxPlayerMoveDeltaTime = 3.0f / 60.0f;

// Input/view components are ~[-1, 1]; anything way past that is garbage and can overflow Normalize()
static constexpr float MaxPlayerMoveAxisMagnitude = 4.0f;

struct PlayerMove
{
    uint32 moveId;
    float deltaTime; // seconds of simulation this move covers
    float movementInput[2];
    float viewDirection[3];
    uint8 jumpRequested;
    uint8 sprintHeld;   // sprint button held this move
    uint8 jumpHeld;     // jump button held this move
    uint8 descendHeld;  // descend button held this move

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

HYP_FORCE_INLINE bool IsPlayerMoveAxisValid(float value)
{
    return std::isfinite(value) && std::fabs(value) <= MaxPlayerMoveAxisMagnitude;
}

// Rejects moves a legit client can't produce (NaN/Inf, negative time, absurd input vectors)
HYP_FORCE_INLINE bool IsPlayerMoveValid(const PlayerMove& move)
{
    if (!std::isfinite(move.deltaTime) || move.deltaTime < 0.0f)
    {
        return false;
    }

    return IsPlayerMoveAxisValid(move.movementInput[0])
        && IsPlayerMoveAxisValid(move.movementInput[1])
        && IsPlayerMoveAxisValid(move.viewDirection[0])
        && IsPlayerMoveAxisValid(move.viewDirection[1])
        && IsPlayerMoveAxisValid(move.viewDirection[2]);
}

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

// Deserializes a batch of moves. Returns the number of valid moves written to outMoves (clamped to maxMoves and to
// what the payload actually holds). Invalid moves are skipped.
HYP_FORCE_INLINE uint32 DeserializePlayerMoves(ByteReader& reader, uint32& outLastAckedMoveId, PlayerMove* outMoves, uint32 maxMoves)
{
    uint8 numMoves = 0;
    outLastAckedMoveId = 0;

    if (reader.Position() + sizeof(uint32) + sizeof(uint8) > reader.Max())
    {
        return 0;
    }

    reader.Read(&outLastAckedMoveId, sizeof(uint32));
    reader.Read(&numMoves, sizeof(uint8));

    const uint32 numMovesInPayload = uint32((reader.Max() - reader.Position()) / sizeof(PlayerMove));
    const uint32 count = MathUtil::Min(MathUtil::Min(uint32(numMoves), maxMoves), numMovesInPayload);

    uint32 numValidMoves = 0;

    for (uint32 i = 0; i < count; ++i)
    {
        PlayerMove move;
        reader.Read(&move, sizeof(PlayerMove));

        if (!IsPlayerMoveValid(move))
        {
            continue;
        }

        outMoves[numValidMoves++] = move;
    }

    return numValidMoves;
}

HYP_FORCE_INLINE void SerializePlayerMoveAck(ByteWriter& writer, const PlayerMoveAck& ack)
{
    writer.Write(&ack, sizeof(ack));
}

HYP_FORCE_INLINE PlayerMoveAck DeserializePlayerMoveAck(ByteReader& reader)
{
    PlayerMoveAck ack {};
    reader.Read(&ack, sizeof(ack));

    return ack;
}

} // namespace Hyperion
