#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region GameState Reflection Data

HYP_BEGIN_STRUCT(GameState, 368, 0, {})
    HypField(NAME(HYP_STR(Mode)), &GameState::mode, offsetof(GameState, mode)),
    HypField(NAME(HYP_STR(DeltaTime)), &GameState::deltaTime, offsetof(GameState, deltaTime)),
    HypField(NAME(HYP_STR(GameTime)), &GameState::gameTime, offsetof(GameState, gameTime)),
    HypMethod(NAME(HYP_STR(IsEditor)), &GameState::IsEditor),
    HypMethod(NAME(HYP_STR(IsSimulating)), &GameState::IsSimulating)
HYP_END_STRUCT

#pragma endregion GameState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GameStateMode Reflection Data

HYP_BEGIN_ENUM(GameStateMode, 369, 0, {})
    HypConstant(NAME(HYP_STR(EDITOR)), GameStateMode::EDITOR),
    HypConstant(NAME(HYP_STR(SIMULATING)), GameStateMode::SIMULATING)
HYP_END_ENUM

#pragma endregion GameStateMode Reflection Data

} // namespace hyperion

