/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_ENUM()
enum class CharacterFacingMode : uint32
{
    None = 0,          //!< @title="None"
    MovementDirection, //!< @title="Movement Direction" @description="Face movement direction"
    ViewDirection      //!< @title="View Direction" @description="Face camera look direction"
};

HYP_STRUCT()
struct CharacterModelAnimations
{
    HYP_STRUCT_BODY(CharacterModelAnimations);

    HYP_FIELD(Property = "IdleAnimation", Serialize, Editor, Title = "Idle Animation")
    Name idleAnimation = NAME("Idle");

    HYP_FIELD(Property = "WalkAnimation", Serialize, Editor, Title = "Walk Animation")
    Name walkAnimation = NAME("Walk");

    HYP_FIELD(Property = "RunAnimation", Serialize, Editor, Title = "Run Animation")
    Name runAnimation = NAME("Run");

    HYP_FIELD(Property = "StrafeLeftAnimation", Serialize, Editor, Title = "Strafe Left Animation")
    Name strafeLeftAnimation = NAME("StrafeLeft");

    HYP_FIELD(Property = "StrafeRightAnimation", Serialize, Editor, Title = "Strafe Right Animation")
    Name strafeRightAnimation = NAME("StrafeRight");

    HYP_FIELD(Property = "WalkBackwardAnimation", Serialize, Editor, Title = "Walk Backward Animation")
    Name walkBackwardAnimation = NAME("WalkBackward");

    HYP_FIELD(Property = "JumpWindupAnimation", Serialize, Editor, Title = "Jump Windup Animation")
    Name jumpWindupAnimation = NAME("JumpWindup");

    HYP_FIELD(Property = "JumpAnimation", Serialize, Editor, Title = "Jump Animation")
    Name jumpAnimation = NAME("Jump");

    HYP_FIELD(Property = "FallAnimation", Serialize, Editor, Title = "Fall Animation")
    Name fallAnimation = NAME("Fall");

    HYP_FIELD(Property = "LandAnimation", Serialize, Editor, Title = "Land Animation")
    Name landAnimation = NAME("Land");

    HYP_FIELD(Property = "RunJumpAnimation", Serialize, Editor, Title = "Run Jump Animation")
    Name runJumpAnimation = NAME("RunJump");

    HYP_FIELD(Property = "RunFallAnimation", Serialize, Editor, Title = "Run Fall Animation")
    Name runFallAnimation = NAME("RunFall");

    HYP_FIELD(Property = "RunLandAnimation", Serialize, Editor, Title = "Run Land Animation")
    Name runLandAnimation = NAME("RunLand");

    HYP_FIELD(Property = "AimTwistRootBone", Serialize, Editor, Title = "Aim Twist Root Bone")
    Name aimTwistRootBone = NAME("spine_01");

    HYP_FIELD(Property = "AimTwistEndBone", Serialize, Editor, Title = "Aim Twist End Bone")
    Name aimTwistEndBone = NAME("spine_03");

    HYP_FIELD(Property = "AimRange", Serialize, Editor, Title = "Aim Range")
    float aimRange = 60.0f;

    HYP_FIELD(Property = "TurnLeftAnimation", Serialize, Editor, Title = "Turn Left Animation")
    Name turnLeftAnimation = NAME("TurnLeft");

    HYP_FIELD(Property = "TurnRightAnimation", Serialize, Editor, Title = "Turn Right Animation")
    Name turnRightAnimation = NAME("TurnRight");

    HYP_FIELD(Property = "TurnStepAngle", Serialize, Editor, Title = "Turn Step Angle")
    float turnStepAngle = 90.0f;

    HYP_FIELD(Property = "TurnStepStartAngle", Serialize, Editor, Title = "Turn Step Start Angle")
    float turnStepStartAngle = 45.0f;

    HYP_FIELD(Property = "TurnLeftSmallAnimation", Serialize, Editor, Title = "Turn Left Small Animation")
    Name turnLeftSmallAnimation = NAME("TurnLeftSmall");

    HYP_FIELD(Property = "TurnRightSmallAnimation", Serialize, Editor, Title = "Turn Right Small Animation")
    Name turnRightSmallAnimation = NAME("TurnRightSmall");

    HYP_FIELD(Property = "TurnStepSmallAngle", Serialize, Editor, Title = "Turn Step Small Angle")
    float turnStepSmallAngle = 45.0f;

    HYP_FIELD(Property = "SettleAngle", Serialize, Editor, Title = "Settle Angle")
    float settleAngle = 25.0f;

    HYP_FIELD(Property = "WalkReferenceSpeed", Serialize, Editor, Title = "Walk Reference Speed")
    float walkReferenceSpeed = 1.02f;

    HYP_FIELD(Property = "RunReferenceSpeed", Serialize, Editor, Title = "Run Reference Speed")
    float runReferenceSpeed = 8.6f;

    HYP_FIELD(Property = "WeaponOverlayRootBone", Serialize, Editor, Title = "Weapon Overlay Root Bone")
    Name weaponOverlayRootBone = NAME("clavicle_l");

    HYP_FIELD(Property = "WeaponOverlaySecondRootBone", Serialize, Editor, Title = "Weapon Overlay Second Root Bone")
    Name weaponOverlaySecondRootBone = NAME("clavicle_r");
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

    HYP_FIELD(Property = "Animations", Serialize, Editor)
    CharacterModelAnimations animations;

    HYP_FIELD(Property = "SpeedSmoothing", Serialize, Editor, Title = "Speed Smoothing")
    float speedSmoothing = 0.15f;

    HYP_FIELD(Property = "HeldWeaponAnimation", Serialize, Editor, Title = "Held Weapon Animation")
    Name heldWeaponAnimation;

    HYP_FIELD(Property = "WeaponBlendTime", Serialize, Editor, Title = "Weapon Blend Time")
    float weaponBlendTime = 0.25f;

    HYP_FIELD(Transient)
    Name shownWeaponAnimation;

    HYP_FIELD(Transient)
    float weaponWeight = 0.0f;

    HYP_FIELD(Transient)
    float weaponTime = 0.0f;

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
    float locomotionPhase = 0.0f;

    HYP_FIELD(Transient)
    float idleTime = 0.0f;

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
    float landTime = -1.0f;

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
    float turnStepProgress = 0.0f;

    HYP_FIELD(Transient)
    float turnStepWeight = 0.0f;

    HYP_FIELD(Transient)
    float aimTwist = 0.0f;
};

} // namespace Hyperion
