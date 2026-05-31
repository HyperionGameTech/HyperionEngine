/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

#include <Physics/PhysicsMaterial.hpp>

namespace Hyperion {

class RigidBody;
class PhysicsShape;

HYP_STRUCT(Component,
    Label = "Rigid Body Component",
    Description = "Allows an entity to have an associated RigidBody for physics simulation",
    Editor = true)
struct RigidBodyComponent
{
    HYP_STRUCT_BODY(RigidBodyComponent);

    HYP_FIELD(Property = "PhysicsMaterial", Serialize, Editor)
    PhysicsMaterial physicsMaterial;

    HYP_FIELD(Property = "CollisionShape", Serialize, Editor)
    Handle<PhysicsShape> shape;

    HYP_FIELD(Property = "RigidBody", Transient)
    Handle<RigidBody> rigidBody;
};

} // namespace Hyperion
