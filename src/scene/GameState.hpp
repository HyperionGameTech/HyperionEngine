/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/reflection/HypObjectMacros.hpp>

namespace hyperion {

HYP_ENUM()
enum class GameStateMode : uint32
{
    EDITOR = 0,
    SIMULATING = 1
};

HYP_STRUCT()
struct GameState
{
    HYP_STRUCT_BODY(GameState);

    HYP_FIELD()
    GameStateMode mode = GameStateMode::EDITOR;

    HYP_FIELD()
    float deltaTime = 0.0;

    HYP_FIELD()
    float gameTime = 0.0;

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsEditor() const
    {
        return mode == GameStateMode::EDITOR;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsSimulating() const
    {
        return mode == GameStateMode::SIMULATING;
    }
};

} // namespace hyperion
