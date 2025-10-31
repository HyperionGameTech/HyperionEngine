#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region UIComponent Reflection Data

HYP_BEGIN_STRUCT(UIComponent, 398, 0, {}, HypClassAttribute("component", true),HypClassAttribute("size", 8),HypClassAttribute("serialize", false))
    HypField(NAME(HYP_STR(UiObject)), &UIComponent::uiObject, offsetof(UIComponent, uiObject))
HYP_END_STRUCT

#pragma endregion UIComponent Reflection Data

HYP_REGISTER_COMPONENT(UIComponent);
static_assert(sizeof(UIComponent) == 8, "Expected sizeof(UIComponent) to be 8 bytes");
} // namespace hyperion

