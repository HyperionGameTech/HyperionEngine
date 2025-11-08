#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Game Reflection Data

HYP_BEGIN_CLASS(Game, 53, 1, NAME("ObjectBase"), ClassAttribute("abstract", true))
    Method(NAME(HYP_STR(GetWorld)), &Game::GetWorld)
HYP_END_CLASS

#pragma endregion Game Reflection Data

} // namespace hyperion

