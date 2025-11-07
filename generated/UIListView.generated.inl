#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIListViewItem Reflection Data

HYP_BEGIN_CLASS(UIListViewItem, 198, 0, NAME("UIObject"))
HYP_END_CLASS

#pragma endregion UIListViewItem Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIListView Reflection Data

HYP_BEGIN_CLASS(UIListView, 208, 0, NAME("UIPanel"))
    Method(NAME(HYP_STR(SetSelectedItem)), &UIListView::SetSelectedItem),
    Method(NAME(HYP_STR(GetSelectedItemIndex)), &UIListView::GetSelectedItemIndex),
    Method(NAME(HYP_STR(SetSelectedItemIndex)), &UIListView::SetSelectedItemIndex),
    Method(NAME(HYP_STR(GetOrientation)), &UIListView::GetOrientation, Span<const ClassAttribute> { {ClassAttribute("property", "Orientation") } }),
    Method(NAME(HYP_STR(SetOrientation)), &UIListView::SetOrientation, Span<const ClassAttribute> { {ClassAttribute("property", "Orientation") } })
HYP_END_CLASS

#pragma endregion UIListView Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIListViewOrientation Reflection Data

HYP_BEGIN_ENUM(UIListViewOrientation, 407, 0, {})
    StaticField(NAME(HYP_STR(VERTICAL)), UIListViewOrientation::VERTICAL),
    StaticField(NAME(HYP_STR(HORIZONTAL)), UIListViewOrientation::HORIZONTAL)
HYP_END_ENUM

#pragma endregion UIListViewOrientation Reflection Data

} // namespace hyperion

