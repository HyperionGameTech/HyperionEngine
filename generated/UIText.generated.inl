#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIText Reflection Data

HYP_BEGIN_CLASS(UIText, 216, 0, NAME("UIObject"))
    Method(NAME(HYP_STR(GetCharacterOffset)), &UIText::GetCharacterOffset)
HYP_END_CLASS

#pragma endregion UIText Reflection Data

} // namespace hyperion

