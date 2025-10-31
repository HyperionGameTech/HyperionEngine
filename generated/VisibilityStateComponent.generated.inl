#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region VisibilityStateFlags Reflection Data

HYP_BEGIN_ENUM(VisibilityStateFlags, 392, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), VisibilityStateFlags::NONE),
    HypConstant(NAME(HYP_STR(ALWAYS_VISIBLE)), VisibilityStateFlags::ALWAYS_VISIBLE),
    HypConstant(NAME(HYP_STR(INVALIDATED)), VisibilityStateFlags::INVALIDATED)
HYP_END_ENUM

#pragma endregion VisibilityStateFlags Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region VisibilityStateComponent Reflection Data

HYP_BEGIN_STRUCT(VisibilityStateComponent, 393, 0, {}, HypClassAttribute("component", true),HypClassAttribute("size", 32),HypClassAttribute("serialize", false),HypClassAttribute("editor", false))
    HypField(NAME(HYP_STR(Flags)), &VisibilityStateComponent::flags, offsetof(VisibilityStateComponent, flags)),
    HypField(NAME(HYP_STR(OctantId)), &VisibilityStateComponent::octantId, offsetof(VisibilityStateComponent, octantId)),
    HypField(NAME(HYP_STR(VisibilityState)), &VisibilityStateComponent::visibilityState, offsetof(VisibilityStateComponent, visibilityState))
HYP_END_STRUCT

#pragma endregion VisibilityStateComponent Reflection Data

HYP_REGISTER_COMPONENT(VisibilityStateComponent);
static_assert(sizeof(VisibilityStateComponent) == 32, "Expected sizeof(VisibilityStateComponent) to be 32 bytes");
} // namespace hyperion

