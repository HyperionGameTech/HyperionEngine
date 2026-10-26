/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

class AnimationGraph;

HYP_ENUM()
enum class CharacterFacingMode : uint32
{
    None = 0,          //!< @title="None"
    MovementDirection, //!< @title="Movement Direction" @description="Face movement direction"
    ViewDirection      //!< @title="View Direction" @description="Face camera look direction"
};

HYP_STRUCT(Component,
    Label = "Character Model Component",
    Editor = true)
struct CharacterModelComponent
{
    HYP_STRUCT_BODY(CharacterModelComponent);

    HYP_FIELD(Property = "AlignToCapsule", Serialize, Editor, Title = "Align To Capsule")
    bool alignToCapsule = true;

    HYP_FIELD(Property = "ModelOffset", Serialize, Editor, Title = "Model Offset")
    Vec3f modelOffset = Vec3f::Zero();

    HYP_FIELD(Property = "FacingMode", Serialize, Editor, Title = "Facing Mode")
    CharacterFacingMode facingMode = CharacterFacingMode::MovementDirection;

    HYP_FIELD(Property = "TurnSharpness", Serialize, Editor, Title = "Turn Sharpness")
    float turnSharpness = 12.0f;

    HYP_FIELD(Property = "ForwardYawOffset", Serialize, Editor, Title = "Forward Yaw Offset")
    float forwardYawOffset = 0.0f;

    HYP_FIELD(Property = "SpeedSmoothing", Serialize, Editor, Title = "Speed Smoothing")
    float speedSmoothing = 0.15f;

    // the graph the model's skinned meshes run, fed the character's motion as parameters; the engine's HumanoidLocomotion when unset
    HYP_FIELD(Property = "Graph", Serialize, Editor, Title = "Animation Graph")
    Handle<AnimationGraph> graph;

    HYP_FIELD(Property = "AimRange", Serialize, Editor, Title = "Aim Range")
    float aimRange = 60.0f;

    HYP_FIELD(Property = "TurnStepAngle", Serialize, Editor, Title = "Turn Step Angle")
    float turnStepAngle = 90.0f;

    HYP_FIELD(Property = "TurnStepStartAngle", Serialize, Editor, Title = "Turn Step Start Angle")
    float turnStepStartAngle = 45.0f;

    HYP_FIELD(Property = "TurnStepSmallAngle", Serialize, Editor, Title = "Turn Step Small Angle")
    float turnStepSmallAngle = 45.0f;

    HYP_FIELD(Property = "SettleAngle", Serialize, Editor, Title = "Settle Angle")
    float settleAngle = 25.0f;

    // standing still, for turn steps, is below 30% of this
    HYP_FIELD(Property = "WalkReferenceSpeed", Serialize, Editor, Title = "Walk Reference Speed")
    float walkReferenceSpeed = 1.02f;

    // feet follow uneven ground through the graph's foot IK, probing this far above and below the capsule's floor
    HYP_FIELD(Property = "FootIK", Serialize, Editor, Title = "Foot IK")
    bool footIK = true;

    HYP_FIELD(Property = "FootProbeHeight", Serialize, Editor, Title = "Foot Probe Height")
    float footProbeHeight = 0.5f;

    HYP_FIELD(Property = "FootProbeDepth", Serialize, Editor, Title = "Foot Probe Depth")
    float footProbeDepth = 0.5f;

    HYP_FIELD(Property = "FootIKSmoothing", Serialize, Editor, Title = "Foot IK Smoothing")
    float footIKSmoothing = 0.08f;

    // root motion clips move the character controller instead of its input
    HYP_FIELD(Property = "ApplyRootMotion", Serialize, Editor, Title = "Apply Root Motion")
    bool applyRootMotion = false;

    HYP_FIELD(Transient)
    Vec3f previousTranslation;

    HYP_FIELD(Transient)
    bool hasPreviousTranslation = false;

    HYP_FIELD(Transient)
    float smoothedSpeed = 0.0f;

    HYP_FIELD(Transient)
    Vec2f smoothedLocalVelocity = Vec2f::Zero();

    HYP_FIELD(Transient)
    float moveAngle = 0.0f;

    HYP_FIELD(Transient)
    float facingYaw = 0.0f;

    HYP_FIELD(Transient)
    bool hasFacingYaw = false;

    HYP_FIELD(Transient)
    bool isAirborne = false;

    HYP_FIELD(Transient)
    bool isJumping = false;

    HYP_FIELD(Transient)
    bool isRunningJump = false;

    HYP_FIELD(Transient)
    float airTime = 0.0f;

    HYP_FIELD(Transient)
    float jumpWindup = -1.0f;

    HYP_FIELD(Transient)
    bool isJumpWoundUp = false;

    HYP_FIELD(Transient)
    float jumpWindupHoldTime = 0.0f;

    HYP_FIELD(Transient)
    float verticalSpeed = 0.0f;

    HYP_FIELD(Transient)
    float bodyYaw = 0.0f;

    HYP_FIELD(Transient)
    bool hasBodyYaw = false;

    HYP_FIELD(Transient)
    bool isTurnStepping = false;

    HYP_FIELD(Transient)
    bool isTurnStepLeft = false;

    HYP_FIELD(Transient)
    float turnStepFromYaw = 0.0f;

    HYP_FIELD(Transient)
    float turnStepSize = 0.0f;

    HYP_FIELD(Transient)
    bool isTurnStepSmall = false;

    HYP_FIELD(Transient)
    float viewStillTime = 0.0f;

    HYP_FIELD(Transient)
    float lastFacingYaw = 0.0f;

    HYP_FIELD(Transient)
    float turnStepTime = 0.0f;

    HYP_FIELD(Transient)
    float turnStepWeight = 0.0f;

    HYP_FIELD(Transient)
    float aimTwist = 0.0f;

    HYP_FIELD(Transient)
    float leftFootOffset = 0.0f;

    HYP_FIELD(Transient)
    float rightFootOffset = 0.0f;
};

} // namespace Hyperion
