#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region GameState Reflection Data

HYP_BEGIN_STRUCT(GameState, 358, 0, {})
    Field(NAME(HYP_STR(Mode)), &GameState::mode, offsetof(GameState, mode)),
    Field(NAME(HYP_STR(DeltaTime)), &GameState::deltaTime, offsetof(GameState, deltaTime)),
    Field(NAME(HYP_STR(GameTime)), &GameState::gameTime, offsetof(GameState, gameTime)),
    Method(NAME(HYP_STR(IsEditor)), &GameState::IsEditor),
    Method(NAME(HYP_STR(IsSimulating)), &GameState::IsSimulating)
HYP_END_STRUCT

#pragma endregion GameState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GameStateMode Reflection Data

HYP_BEGIN_ENUM(GameStateMode, 359, 0, {})
    StaticField(NAME(HYP_STR(EDITOR)), GameStateMode::EDITOR),
    StaticField(NAME(HYP_STR(SIMULATING)), GameStateMode::SIMULATING)
HYP_END_ENUM

#pragma endregion GameStateMode Reflection Data

} // namespace hyperion

