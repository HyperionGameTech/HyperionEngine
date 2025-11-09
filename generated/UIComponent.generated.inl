#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region UIComponent Reflection Data

HYP_BEGIN_STRUCT(UIComponent, 387, 0, {}, ClassAttribute("component", true),ClassAttribute("size", 8),ClassAttribute("serialize", false))
    Field(NAME(HYP_STR(UiObject)), &UIComponent::uiObject, offsetof(UIComponent, uiObject))
HYP_END_STRUCT

#pragma endregion UIComponent Reflection Data

HYP_REGISTER_COMPONENT(UIComponent);
static_assert(sizeof(UIComponent) == 8, "Expected sizeof(UIComponent) to be 8 bytes");
} // namespace hyperion

