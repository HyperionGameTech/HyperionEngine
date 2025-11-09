#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region PhysicsShapeType Reflection Data

HYP_BEGIN_ENUM(PhysicsShapeType, 365, 0, {})
    StaticField(NAME(HYP_STR(NONE)), PhysicsShapeType::NONE),
    StaticField(NAME(HYP_STR(BOX)), PhysicsShapeType::BOX),
    StaticField(NAME(HYP_STR(SPHERE)), PhysicsShapeType::SPHERE),
    StaticField(NAME(HYP_STR(PLANE)), PhysicsShapeType::PLANE),
    StaticField(NAME(HYP_STR(CONVEX_HULL)), PhysicsShapeType::CONVEX_HULL)
HYP_END_ENUM

#pragma endregion PhysicsShapeType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RigidBody Reflection Data

HYP_BEGIN_CLASS(RigidBody, 140, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetTransform)), &RigidBody::GetTransform, Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Transform") } }),
    Method(NAME(HYP_STR(SetTransform)), &RigidBody::SetTransform, Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Transform") } }),
    Method(NAME(HYP_STR(GetShape)), &RigidBody::GetShape, Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Shape") } }),
    Method(NAME(HYP_STR(SetShape)), &RigidBody::SetShape, Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Shape") } }),
    Method(NAME(HYP_STR(IsKinematic)), &RigidBody::IsKinematic, Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "IsKinematic") } }),
    Method(NAME(HYP_STR(SetIsKinematic)), &RigidBody::SetIsKinematic, Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "IsKinematic") } }),
    Method(NAME(HYP_STR(ApplyForce)), &RigidBody::ApplyForce)
HYP_END_CLASS

#pragma endregion RigidBody Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SpherePhysicsShape Reflection Data

HYP_BEGIN_CLASS(SpherePhysicsShape, 142, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion SpherePhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ConvexHullPhysicsShape Reflection Data

HYP_BEGIN_CLASS(ConvexHullPhysicsShape, 143, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion ConvexHullPhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region BoxPhysicsShape Reflection Data

HYP_BEGIN_CLASS(BoxPhysicsShape, 144, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion BoxPhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region PhysicsShape Reflection Data

HYP_BEGIN_CLASS(PhysicsShape, 141, 4, NAME("ObjectBase"), ClassAttribute("abstract", true))
HYP_END_CLASS

#pragma endregion PhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region PlanePhysicsShape Reflection Data

HYP_BEGIN_CLASS(PlanePhysicsShape, 145, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion PlanePhysicsShape Reflection Data

} // namespace hyperion

