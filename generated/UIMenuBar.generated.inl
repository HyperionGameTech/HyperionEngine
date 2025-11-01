#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UIMenuBarDropDirection Reflection Data

HYP_BEGIN_ENUM(UIMenuBarDropDirection, 407, 0, {})
    HypConstant(NAME(HYP_STR(DOWN)), UIMenuBarDropDirection::DOWN),
    HypConstant(NAME(HYP_STR(UP)), UIMenuBarDropDirection::UP)
HYP_END_ENUM

#pragma endregion UIMenuBarDropDirection Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIMenuItem Reflection Data

HYP_BEGIN_CLASS(UIMenuItem, 198, 0, NAME("UIObject"))
HYP_END_CLASS

#pragma endregion UIMenuItem Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region UIMenuBar Reflection Data

HYP_BEGIN_CLASS(UIMenuBar, 208, 0, NAME("UIPanel"))
    HypMethod(NAME(HYP_STR(GetDropDirection)), &UIMenuBar::GetDropDirection, Span<const HypClassAttribute> { {HypClassAttribute("property", "DropDirection"), HypClassAttribute("xmlattribute", "direction") } }),
    HypMethod(NAME(HYP_STR(SetDropDirection)), &UIMenuBar::SetDropDirection, Span<const HypClassAttribute> { {HypClassAttribute("property", "DropDirection"), HypClassAttribute("xmlattribute", "direction") } }),
    HypMethod(NAME(HYP_STR(SetSelectedMenuItemIndex)), &UIMenuBar::SetSelectedMenuItemIndex),
    HypMethod(NAME(HYP_STR(AddMenuItem)), &UIMenuBar::AddMenuItem),
    HypMethod(NAME(HYP_STR(GetMenuItem)), &UIMenuBar::GetMenuItem),
    HypMethod(NAME(HYP_STR(GetMenuItemIndex)), &UIMenuBar::GetMenuItemIndex),
    HypMethod(NAME(HYP_STR(RemoveMenuItem)), &UIMenuBar::RemoveMenuItem)
HYP_END_CLASS

#pragma endregion UIMenuBar Reflection Data

} // namespace hyperion

