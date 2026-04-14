/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <physics/PhysicsMaterial.hpp>

namespace Hyperion {

class RigidBody;

HYP_STRUCT(Component,
    Label = "Rigid Body Component",
    Description = "Allows an entity to have an associated RigidBody for physics simulation",
    Editor = true)
struct RigidBodyComponent
{
    HYP_STRUCT_BODY(RigidBodyComponent);

    HYP_FIELD(Property = "RigidBody")
    Handle<RigidBody> rigidBody;

    HYP_FIELD(Property = "PhysicsMaterial")
    PhysicsMaterial physicsMaterial;
};

} // namespace Hyperion
