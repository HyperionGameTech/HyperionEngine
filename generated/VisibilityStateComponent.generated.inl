#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region VisibilityStateFlags Reflection Data

HYP_BEGIN_ENUM(VisibilityStateFlags, 389, 0, {})
    StaticField(NAME(HYP_STR(NONE)), VisibilityStateFlags::NONE),
    StaticField(NAME(HYP_STR(ALWAYS_VISIBLE)), VisibilityStateFlags::ALWAYS_VISIBLE),
    StaticField(NAME(HYP_STR(INVALIDATED)), VisibilityStateFlags::INVALIDATED)
HYP_END_ENUM

#pragma endregion VisibilityStateFlags Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region VisibilityStateComponent Reflection Data

HYP_BEGIN_STRUCT(VisibilityStateComponent, 390, 0, {}, ClassAttribute("component", true),ClassAttribute("size", 32),ClassAttribute("serialize", false),ClassAttribute("editor", false))
    Field(NAME(HYP_STR(Flags)), &VisibilityStateComponent::flags, offsetof(VisibilityStateComponent, flags)),
    Field(NAME(HYP_STR(OctantId)), &VisibilityStateComponent::octantId, offsetof(VisibilityStateComponent, octantId)),
    Field(NAME(HYP_STR(VisibilityState)), &VisibilityStateComponent::visibilityState, offsetof(VisibilityStateComponent, visibilityState))
HYP_END_STRUCT

#pragma endregion VisibilityStateComponent Reflection Data

HYP_REGISTER_COMPONENT(VisibilityStateComponent);
static_assert(sizeof(VisibilityStateComponent) == 32, "Expected sizeof(VisibilityStateComponent) to be 32 bytes");
} // namespace hyperion

