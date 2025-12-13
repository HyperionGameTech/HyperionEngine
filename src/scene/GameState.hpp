/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/reflection/ObjectMacros.hpp>

namespace hyperion {

HYP_ENUM()
enum class GameStateMode : uint32
{
    STOPPED = 0,
    SIMULATING,
    PAUSED,
    EDITOR
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

} // namespace hyperion
