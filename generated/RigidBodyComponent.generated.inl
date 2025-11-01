#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region RigidBodyComponent Reflection Data

HYP_BEGIN_STRUCT(RigidBodyComponent, 385, 0, {}, HypClassAttribute("component", true),HypClassAttribute("label", "Rigid Body Component"),HypClassAttribute("description", "Controls the properties of an object with rigid body physics."),HypClassAttribute("editor", true))
    HypField(NAME(HYP_STR(RigidBody)), &RigidBodyComponent::rigidBody, offsetof(RigidBodyComponent, rigidBody), Span<const HypClassAttribute> { {HypClassAttribute("property", "RigidBody") } }),
    HypField(NAME(HYP_STR(PhysicsMaterial)), &RigidBodyComponent::physicsMaterial, offsetof(RigidBodyComponent, physicsMaterial), Span<const HypClassAttribute> { {HypClassAttribute("property", "PhysicsMaterial") } }),
    HypField(NAME(HYP_STR(Flags)), &RigidBodyComponent::flags, offsetof(RigidBodyComponent, flags), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(TransformHashCode)), &RigidBodyComponent::transformHashCode, offsetof(RigidBodyComponent, transformHashCode), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion RigidBodyComponent Reflection Data

HYP_REGISTER_COMPONENT(RigidBodyComponent);
} // namespace hyperion


namespace hyperion {

#pragma region RigidBodyComponentFlags Reflection Data

HYP_BEGIN_ENUM(RigidBodyComponentFlags, 386, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), RigidBodyComponentFlags::NONE),
    HypConstant(NAME(HYP_STR(INIT)), RigidBodyComponentFlags::INIT),
    HypConstant(NAME(HYP_STR(DIRTY)), RigidBodyComponentFlags::DIRTY)
HYP_END_ENUM

#pragma endregion RigidBodyComponentFlags Reflection Data

} // namespace hyperion

