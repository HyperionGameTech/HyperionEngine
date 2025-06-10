/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_STATE_HPP
#define HYPERION_GAME_STATE_HPP

#include <core/Defines.hpp>

namespace hyperion {

enum class GameStateMode : uint32
{
    EDITOR = 0,
    SIMULATING = 1
};

HYP_STRUCT()
struct GameState
{
    HYP_FIELD()
    GameStateMode mode = GameStateMode::EDITOR;

    HYP_FIELD()
    float delta_time = 0.0;

    HYP_FIELD()
    float game_time = 0.0;

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

#endif
