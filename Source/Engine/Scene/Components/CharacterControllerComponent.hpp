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

HYP_STRUCT()
struct CharacterMovementSettings
{
    HYP_STRUCT_BODY(CharacterMovementSettings);

    HYP_FIELD(Property = "MoveSpeed", Serialize, Title = "Move Speed", Description = "Target ground speed in m/s")
    float moveSpeed = 15.0f;

    HYP_FIELD(Property = "SprintSpeed", Serialize, Title = "Sprint Speed", Description = "Target ground speed while sprinting (hold Shift)")
    float sprintSpeed = 25.0f;

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

    HYP_FIELD(Property = "SprintAcceleration", Serialize, Title = "Sprint Acceleration")
    float sprintAcceleration = 7.0f;

    HYP_FIELD(Property = "SprintTurnRate", Serialize, Title = "Sprint Turn Rate")
    float sprintTurnRate = 140.0f;

    HYP_FIELD(Property = "TurnSpeedLoss", Serialize, Title = "Turn Speed Loss")
    float turnSpeedLoss = 1.5f;

    HYP_FIELD(Property = "BrakeDeceleration", Serialize, Title = "Brake Deceleration")
    float brakeDeceleration = 22.0f;
};

HYP_STRUCT()
struct CharacterJumpSettings
{
    HYP_STRUCT_BODY(CharacterJumpSettings);

    HYP_FIELD(Property = "Speed", Serialize, Title = "Speed")
    float speed = 4.9f;

    HYP_FIELD(Property = "CutGravityMultiplier", Serialize, Title = "Cut Gravity Multiplier", Description = "Extra gravity while rising if the jump button was released early (variable jump height)")
    float cutGravityMultiplier = 2.2f;

    HYP_FIELD(Property = "ApexGravityMultiplier", Serialize, Title = "Apex Gravity Multiplier", Description = "Gravity scale near the top of the jump; below 1 gives a little hang time")
    float apexGravityMultiplier = 0.85f;

    HYP_FIELD(Property = "FallGravityMultiplier", Serialize, Title = "Fall Gravity Multiplier", Description = "Gravity scale while falling; above 1 makes descents snappier than the rise")
    float fallGravityMultiplier = 1.8f;

    HYP_FIELD(Property = "FallSpeed", Serialize, Title = "Fall Speed", Description = "Terminal fall speed")
    float fallSpeed = 55.0f;

    HYP_FIELD(Property = "CoyoteTime", Serialize, Title = "Coyote Time", Description = "Time before falling off a ledge")
    float coyoteTime = 0.15f;

    HYP_FIELD(Property = "BufferTime", Serialize, Title = "Buffer Time")
    float bufferTime = 0.15f;

    HYP_FIELD(Property = "WindupTime", Serialize, Title = "Windup Time", Description = "Time spent crouching into a jump before leaving the ground")
    float windupTime = 0.1f;
};

HYP_STRUCT()
struct CharacterPushSettings
{
    HYP_STRUCT_BODY(CharacterPushSettings);

    HYP_FIELD(Property = "MassLimit", Serialize, Title = "Mass Limit")
    float massLimit = 350.0f;

    HYP_FIELD(Property = "MaxSpeed", Serialize, Title = "Max Speed")
    float maxSpeed = 1.5f;

    HYP_FIELD(Property = "SpeedScale", Serialize, Title = "Speed Scale")
    float speedScale = 1.0f;

    HYP_FIELD(Property = "PredictionReleaseDelay", Serialize, Title = "Prediction Release Delay", Description = "Grace period after last contact before a locally-predicted pushed body is handed back to replication")
    float predictionReleaseDelay = 0.25f;

    HYP_FIELD(Property = "MinGroundSupportMass", Serialize, Title = "Min Ground Support Mass", Description = "Dynamic bodies lighter than this won't have their velocity treated as moving-platform footing, so a light pushable prop can't fling the character around when brushed or briefly stood on")
    float minGroundSupportMass = 20.0f;
};

HYP_STRUCT()
struct CharacterShadowBodySettings
{
    HYP_STRUCT_BODY(CharacterShadowBodySettings);

    HYP_FIELD(Property = "MaxSpeed", Serialize)
    float maxSpeed = 60.0f;

    HYP_FIELD(Property = "TeleportDistance", Serialize)
    float teleportDistance = 0.5f;
};

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

    HYP_FIELD(Property = "Movement", Serialize, Title = "Movement")
    CharacterMovementSettings movement;

    HYP_FIELD(Property = "Jump", Serialize, Title = "Jump")
    CharacterJumpSettings jump;

    HYP_FIELD(Property = "Push", Serialize, Title = "Push")
    CharacterPushSettings push;

    HYP_FIELD(Property = "ShadowBody", Serialize, Editor = false)
    CharacterShadowBodySettings shadowBody;

    HYP_FIELD(Transient)
    bool isOnGround = false;

    HYP_FIELD(Transient)
    float jumpWindupRemaining = 0.0f;
};

} // namespace Hyperion
