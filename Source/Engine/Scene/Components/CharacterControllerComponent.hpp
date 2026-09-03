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

struct PlayerMove;

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

    HYP_FIELD(Transient)
    Vec3f viewDirection = Vec3f(0.0f, 0.0f, 1.0f);

    HYP_FIELD(Transient)
    Vec3f translation;

    HYP_FIELD(Property = "MoveSpeed", Serialize, Title = "Move Speed", Description = "Target ground speed in m/s")
    float moveSpeed = 5.0f;

    HYP_FIELD(Property = "SprintSpeed", Serialize, Title = "Sprint Speed", Description = "Target ground speed while sprinting (hold Shift)")
    float sprintSpeed = 7.5f;

    HYP_FIELD(Property = "GroundAcceleration", Serialize, Title = "Ground Acceleration", Description = "How quickly ground speed ramps toward the target (higher = snappier)")
    float groundAcceleration = 12.0f;

    HYP_FIELD(Property = "AirAcceleration", Serialize, Title = "Air Acceleration", Description = "How quickly the character can steer toward the target speed while airborne")
    float airAcceleration = 3.0f;

    HYP_FIELD(Property = "Friction", Serialize, Title = "Friction", Description = "Ground friction; how quickly the character slows down when input is released")
    float friction = 8.0f;

    HYP_FIELD(Property = "StopSpeed", Serialize, Title = "Stop Speed", Description = "Minimum speed friction acts against, keeps the final stop from feeling sluggish")
    float stopSpeed = 2.5f;

    HYP_FIELD(Property = "StepHeight", Serialize, Title = "Step Height")
    float stepHeight = 0.35f;

    HYP_FIELD(Property = "MaxSlopeAngle", Serialize, Title = "Max Slope Angle")
    float maxSlopeAngle = 45.0f;

    HYP_FIELD(Property = "JumpSpeed", Serialize, Title = "Jump Speed")
    float jumpSpeed = 4.9f;

    HYP_FIELD(Property = "JumpCutGravityMultiplier", Serialize, Title = "Jump Cut Gravity Multiplier", Description = "Extra gravity while rising if the jump button was released early (variable jump height)")
    float jumpCutGravityMultiplier = 2.2f;

    HYP_FIELD(Property = "ApexGravityMultiplier", Serialize, Title = "Apex Gravity Multiplier", Description = "Gravity scale near the top of the jump; below 1 gives a little hang time")
    float apexGravityMultiplier = 0.85f;

    HYP_FIELD(Property = "FallGravityMultiplier", Serialize, Title = "Fall Gravity Multiplier", Description = "Gravity scale while falling; above 1 makes descents snappier than the rise")
    float fallGravityMultiplier = 1.8f;

    HYP_FIELD(Property = "FallSpeed", Serialize, Title = "Fall Speed", Description = "Terminal fall speed")
    float fallSpeed = 55.0f;

    HYP_FIELD(Property = "CoyoteTime", Serialize, Title = "Coyote Time", Description = "Time before falling off a ledge")
    float coyoteTime = 0.15f;

    HYP_FIELD(Property = "JumpBufferTime", Serialize, Title = "Jump Buffer Time")
    float jumpBufferTime = 0.15f;

    HYP_FIELD(Property = "ShadowMaxSpeed", Serialize, Editor = false)
    float shadowMaxSpeed = 60.0f;

    HYP_FIELD(Property = "ShadowTeleportDistance", Serialize, Editor = false)
    float shadowTeleportDistance = 0.5f;

    HYP_FIELD(Property = "PushMassLimit", Serialize, Title = "Push Mass Limit")
    float pushMassLimit = 350.0f;

    HYP_FIELD(Property = "MaxPushSpeed", Serialize, Title = "Max Push Speed")
    float maxPushSpeed = 1.5f;

    HYP_FIELD(Property = "PushSpeedScale", Serialize, Title = "Push Speed Scale")
    float pushSpeedScale = 1.0f;

    HYP_FIELD(Property = "PushPredictionReleaseDelay", Serialize, Title = "Push Prediction Release Delay", Description = "Grace period after last contact before a locally-predicted pushed body is handed back to replication")
    float pushPredictionReleaseDelay = 0.25f;

    HYP_FIELD(Property = "MinGroundSupportMass", Serialize, Title = "Min Ground Support Mass", Description = "Dynamic bodies lighter than this won't have their velocity treated as moving-platform footing, so a light pushable prop can't fling the character around when brushed or briefly stood on")
    float minGroundSupportMass = 20.0f;

    HYP_FIELD(Transient)
    bool isOnGround = false;
};

} // namespace Hyperion
