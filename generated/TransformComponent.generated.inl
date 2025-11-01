#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region TransformComponent Reflection Data

HYP_BEGIN_STRUCT(TransformComponent, 390, 0, {}, HypClassAttribute("component", true),HypClassAttribute("label", "Transform Component"),HypClassAttribute("description", "Controls the translation, rotation, and scale of an object."),HypClassAttribute("editor", false),HypClassAttribute("serialize", false))
    HypField(NAME(HYP_STR(Transform)), &TransformComponent::transform, offsetof(TransformComponent, transform), Span<const HypClassAttribute> { {HypClassAttribute("property", "Transform") } })
HYP_END_STRUCT

#pragma endregion TransformComponent Reflection Data

HYP_REGISTER_COMPONENT(TransformComponent);
} // namespace hyperion

