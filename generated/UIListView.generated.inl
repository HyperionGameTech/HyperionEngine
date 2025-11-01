#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIListViewItem Reflection Data

HYP_BEGIN_CLASS(UIListViewItem, 197, 0, NAME("UIObject"))
HYP_END_CLASS

#pragma endregion UIListViewItem Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIListView Reflection Data

HYP_BEGIN_CLASS(UIListView, 207, 0, NAME("UIPanel"))
    HypMethod(NAME(HYP_STR(SetSelectedItem)), &UIListView::SetSelectedItem),
    HypMethod(NAME(HYP_STR(GetSelectedItemIndex)), &UIListView::GetSelectedItemIndex),
    HypMethod(NAME(HYP_STR(SetSelectedItemIndex)), &UIListView::SetSelectedItemIndex),
    HypMethod(NAME(HYP_STR(GetOrientation)), &UIListView::GetOrientation, Span<const HypClassAttribute> { {HypClassAttribute("property", "Orientation") } }),
    HypMethod(NAME(HYP_STR(SetOrientation)), &UIListView::SetOrientation, Span<const HypClassAttribute> { {HypClassAttribute("property", "Orientation") } })
HYP_END_CLASS

#pragma endregion UIListView Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIListViewOrientation Reflection Data

HYP_BEGIN_ENUM(UIListViewOrientation, 406, 0, {})
    HypConstant(NAME(HYP_STR(VERTICAL)), UIListViewOrientation::VERTICAL),
    HypConstant(NAME(HYP_STR(HORIZONTAL)), UIListViewOrientation::HORIZONTAL)
HYP_END_ENUM

#pragma endregion UIListViewOrientation Reflection Data

} // namespace hyperion

