#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIPanel Reflection Data

HYP_BEGIN_CLASS(UIPanel, 7, 12, NAME("UIObject"))
    HypMethod(NAME(HYP_STR(IsHorizontalScrollEnabled)), &UIPanel::IsHorizontalScrollEnabled),
    HypMethod(NAME(HYP_STR(IsVerticalScrollEnabled)), &UIPanel::IsVerticalScrollEnabled),
    HypMethod(NAME(HYP_STR(SetIsScrollEnabled)), &UIPanel::SetIsScrollEnabled)
HYP_END_CLASS

#pragma endregion UIPanel Reflection Data

} // namespace hyperion

