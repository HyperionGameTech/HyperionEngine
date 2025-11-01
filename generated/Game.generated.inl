#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Game Reflection Data

HYP_BEGIN_CLASS(Game, 53, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
    HypMethod(NAME(HYP_STR(GetWorld)), &Game::GetWorld)
HYP_END_CLASS

#pragma endregion Game Reflection Data

} // namespace hyperion

