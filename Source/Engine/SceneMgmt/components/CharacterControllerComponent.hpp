/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <physics/RigidBody.hpp>

namespace Hyperion {

class InputHandlerBase;

HYP_STRUCT(Component,
    Label = "Character Controller Component",
    Description = "Allows an entity to have its movement driven by player input and interact with physics",
    Editor = true)
struct CharacterControllerComponent
{
    HYP_STRUCT_BODY(CharacterControllerComponent);

    HYP_FIELD(Property = "Shape", Serialize)
    Handle<PhysicsShape> shape;

    HYP_FIELD(Transient)
    Handle<InputHandlerBase> inputHandler;

    HYP_FIELD(Transient)
    RC<void> physicsHandle;

    HYP_FIELD(Property = "ViewDirection", Serialize)
    Vec3f viewDirection = Vec3f(0.0f, 0.0f, 1.0f);

    HYP_FIELD(Transient)
    Vec3f translation;

    HYP_FIELD(Property = "MoveSpeed", Serialize)
    float moveSpeed = 5.0f;

    HYP_FIELD(Property = "StepHeight", Serialize)
    float stepHeight = 0.35f;

    HYP_FIELD(Property = "MaxSlopeAngle", Serialize)
    float maxSlopeAngle = 45.0f;

    HYP_FIELD(Property = "JumpSpeed", Serialize)
    float jumpSpeed = 10.0f;

    HYP_FIELD(Property = "FallSpeed", Serialize)
    float fallSpeed = 55.0f;

    HYP_FIELD(Transient)
    bool isOnGround = false;
};

} // namespace Hyperion
