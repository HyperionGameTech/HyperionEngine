#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region PhysicsShapeType Reflection Data

HYP_BEGIN_ENUM(PhysicsShapeType, 366, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), PhysicsShapeType::NONE),
    HypConstant(NAME(HYP_STR(BOX)), PhysicsShapeType::BOX),
    HypConstant(NAME(HYP_STR(SPHERE)), PhysicsShapeType::SPHERE),
    HypConstant(NAME(HYP_STR(PLANE)), PhysicsShapeType::PLANE),
    HypConstant(NAME(HYP_STR(CONVEX_HULL)), PhysicsShapeType::CONVEX_HULL)
HYP_END_ENUM

#pragma endregion PhysicsShapeType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RigidBody Reflection Data

HYP_BEGIN_CLASS(RigidBody, 141, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetTransform)), &RigidBody::GetTransform, Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Transform") } }),
    HypMethod(NAME(HYP_STR(SetTransform)), &RigidBody::SetTransform, Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Transform") } }),
    HypMethod(NAME(HYP_STR(GetShape)), &RigidBody::GetShape, Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Shape") } }),
    HypMethod(NAME(HYP_STR(SetShape)), &RigidBody::SetShape, Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Shape") } }),
    HypMethod(NAME(HYP_STR(IsKinematic)), &RigidBody::IsKinematic, Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "IsKinematic") } }),
    HypMethod(NAME(HYP_STR(SetIsKinematic)), &RigidBody::SetIsKinematic, Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "IsKinematic") } }),
    HypMethod(NAME(HYP_STR(ApplyForce)), &RigidBody::ApplyForce)
HYP_END_CLASS

#pragma endregion RigidBody Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region SpherePhysicsShape Reflection Data

HYP_BEGIN_CLASS(SpherePhysicsShape, 143, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion SpherePhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ConvexHullPhysicsShape Reflection Data

HYP_BEGIN_CLASS(ConvexHullPhysicsShape, 144, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion ConvexHullPhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region BoxPhysicsShape Reflection Data

HYP_BEGIN_CLASS(BoxPhysicsShape, 145, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion BoxPhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region PhysicsShape Reflection Data

HYP_BEGIN_CLASS(PhysicsShape, 142, 4, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
HYP_END_CLASS

#pragma endregion PhysicsShape Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region PlanePhysicsShape Reflection Data

HYP_BEGIN_CLASS(PlanePhysicsShape, 146, 0, NAME("PhysicsShape"))
HYP_END_CLASS

#pragma endregion PlanePhysicsShape Reflection Data

} // namespace hyperion

