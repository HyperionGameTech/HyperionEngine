#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region TransformComponent Reflection Data

HYP_BEGIN_STRUCT(TransformComponent, 386, 0, {}, ClassAttribute("component", true),ClassAttribute("label", "Transform Component"),ClassAttribute("description", "Controls the translation, rotation, and scale of an object."),ClassAttribute("editor", false),ClassAttribute("serialize", false))
    Field(NAME(HYP_STR(Transform)), &TransformComponent::transform, offsetof(TransformComponent, transform), Span<const ClassAttribute> { {ClassAttribute("property", "Transform") } })
HYP_END_STRUCT

#pragma endregion TransformComponent Reflection Data

HYP_REGISTER_COMPONENT(TransformComponent);
} // namespace hyperion

