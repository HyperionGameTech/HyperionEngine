#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIText Reflection Data

HYP_BEGIN_CLASS(UIText, 23, 0, NAME("UIObject"))
    HypMethod(NAME(HYP_STR(GetCharacterOffset)), &UIText::GetCharacterOffset)
HYP_END_CLASS

#pragma endregion UIText Reflection Data

} // namespace hyperion

