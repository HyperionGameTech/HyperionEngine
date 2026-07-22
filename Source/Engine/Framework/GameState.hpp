/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_ENUM()
enum class GameStateMode : uint32
{
    STOPPED = 0,
    SIMULATING,
    PAUSED
};

HYP_STRUCT()
struct GameState
{
    HYP_STRUCT_BODY(GameState);

    HYP_FIELD()
    GameStateMode mode = GameStateMode::STOPPED;

    HYP_FIELD()
    float deltaTime = 0.0;

    HYP_FIELD()
    float gameTime = 0.0;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsStopped() const
    {
        return mode == GameStateMode::STOPPED;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsSimulating() const
    {
        return mode == GameStateMode::SIMULATING;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsPaused() const
    {
        return mode == GameStateMode::PAUSED;
    }
};

} // namespace Hyperion
