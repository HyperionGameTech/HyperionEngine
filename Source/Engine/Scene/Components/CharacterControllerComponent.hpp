/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Physics/RigidBody.hpp>

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
    SharedPtr<void> physicsHandle;

    HYP_FIELD(Property = "ViewDirection", Serialize)
    Vec3f viewDirection = Vec3f(0.0f, 0.0f, 1.0f);

    HYP_FIELD(Transient)
    Vec3f translation;

    HYP_FIELD(Property = "MoveSpeed", Serialize)
    float moveSpeed = 0.05f;

    HYP_FIELD(Property = "StepHeight", Serialize)
    float stepHeight = 0.35f;

    HYP_FIELD(Property = "MaxSlopeAngle", Serialize)
    float maxSlopeAngle = 45.0f;

    HYP_FIELD(Property = "JumpSpeed", Serialize)
    float jumpSpeed = 1.0f;

    HYP_FIELD(Property = "FallSpeed", Serialize)
    float fallSpeed = 5.0f;

    HYP_FIELD(Transient)
    bool isOnGround = false;
};

} // namespace Hyperion
