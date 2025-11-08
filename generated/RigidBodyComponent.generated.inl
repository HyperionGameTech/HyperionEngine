#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region RigidBodyComponent Reflection Data

HYP_BEGIN_STRUCT(RigidBodyComponent, 382, 0, {}, ClassAttribute("component", true),ClassAttribute("label", "Rigid Body Component"),ClassAttribute("description", "Controls the properties of an object with rigid body physics."),ClassAttribute("editor", true))
    Field(NAME(HYP_STR(RigidBody)), &RigidBodyComponent::rigidBody, offsetof(RigidBodyComponent, rigidBody), Span<const ClassAttribute> { {ClassAttribute("property", "RigidBody") } }),
    Field(NAME(HYP_STR(PhysicsMaterial)), &RigidBodyComponent::physicsMaterial, offsetof(RigidBodyComponent, physicsMaterial), Span<const ClassAttribute> { {ClassAttribute("property", "PhysicsMaterial") } }),
    Field(NAME(HYP_STR(Flags)), &RigidBodyComponent::flags, offsetof(RigidBodyComponent, flags), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(TransformHashCode)), &RigidBodyComponent::transformHashCode, offsetof(RigidBodyComponent, transformHashCode), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion RigidBodyComponent Reflection Data

HYP_REGISTER_COMPONENT(RigidBodyComponent);
} // namespace hyperion


namespace hyperion {

#pragma region RigidBodyComponentFlags Reflection Data

HYP_BEGIN_ENUM(RigidBodyComponentFlags, 383, 0, {})
    StaticField(NAME(HYP_STR(NONE)), RigidBodyComponentFlags::NONE),
    StaticField(NAME(HYP_STR(INIT)), RigidBodyComponentFlags::INIT),
    StaticField(NAME(HYP_STR(DIRTY)), RigidBodyComponentFlags::DIRTY)
HYP_END_ENUM

#pragma endregion RigidBodyComponentFlags Reflection Data

} // namespace hyperion

