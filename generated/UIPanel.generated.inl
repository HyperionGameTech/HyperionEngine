#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIPanel Reflection Data

HYP_BEGIN_CLASS(UIPanel, 7, 12, NAME("UIObject"))
    Method(NAME(HYP_STR(IsHorizontalScrollEnabled)), &UIPanel::IsHorizontalScrollEnabled),
    Method(NAME(HYP_STR(IsVerticalScrollEnabled)), &UIPanel::IsVerticalScrollEnabled),
    Method(NAME(HYP_STR(SetIsScrollEnabled)), &UIPanel::SetIsScrollEnabled)
HYP_END_CLASS

#pragma endregion UIPanel Reflection Data

} // namespace hyperion

