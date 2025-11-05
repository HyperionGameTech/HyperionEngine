#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UIMenuBarDropDirection Reflection Data

HYP_BEGIN_ENUM(UIMenuBarDropDirection, 217, 0, {})
    StaticField(NAME(HYP_STR(DOWN)), UIMenuBarDropDirection::DOWN),
    StaticField(NAME(HYP_STR(UP)), UIMenuBarDropDirection::UP)
HYP_END_ENUM

#pragma endregion UIMenuBarDropDirection Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIMenuItem Reflection Data

HYP_BEGIN_CLASS(UIMenuItem, 20, 0, NAME("UIObject"))
HYP_END_CLASS

#pragma endregion UIMenuItem Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIMenuBar Reflection Data

HYP_BEGIN_CLASS(UIMenuBar, 11, 0, NAME("UIPanel"))
    Method(NAME(HYP_STR(GetDropDirection)), &UIMenuBar::GetDropDirection, Span<const ClassAttribute> { {ClassAttribute("property", "DropDirection"), ClassAttribute("xmlattribute", "direction") } }),
    Method(NAME(HYP_STR(SetDropDirection)), &UIMenuBar::SetDropDirection, Span<const ClassAttribute> { {ClassAttribute("property", "DropDirection"), ClassAttribute("xmlattribute", "direction") } }),
    Method(NAME(HYP_STR(SetSelectedMenuItemIndex)), &UIMenuBar::SetSelectedMenuItemIndex),
    Method(NAME(HYP_STR(AddMenuItem)), &UIMenuBar::AddMenuItem),
    Method(NAME(HYP_STR(GetMenuItem)), &UIMenuBar::GetMenuItem),
    Method(NAME(HYP_STR(GetMenuItemIndex)), &UIMenuBar::GetMenuItemIndex),
    Method(NAME(HYP_STR(RemoveMenuItem)), &UIMenuBar::RemoveMenuItem)
HYP_END_CLASS

#pragma endregion UIMenuBar Reflection Data

} // namespace hyperion

