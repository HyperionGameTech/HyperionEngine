/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Name/Name.hpp>

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

    HYP_FIELD(Property = "JumpAnimation", Serialize, Editor, Title = "Jump Animation", Description = "Played once on takeoff, then blends into the fall animation")
    Name jumpAnimation = NAME("Jump");

    HYP_FIELD(Property = "FallAnimation", Serialize, Editor, Title = "Fall Animation", Description = "Looped while airborne")
    Name fallAnimation = NAME("Fall");

    HYP_FIELD(Property = "LandAnimation", Serialize, Editor, Title = "Land Animation", Description = "Played once on touchdown, blending back into locomotion")
    Name landAnimation = NAME("Land");

    HYP_FIELD(Property = "AimAnimation", Serialize, Editor, Title = "Aim Animation", Description = "Standing pose with the upper body twisted from Aim Range right (start) to Aim Range left (end) of the feet")
    Name aimAnimation = NAME("IdleAim");

    HYP_FIELD(Property = "AimRange", Serialize, Editor, Title = "Aim Range", Description = "Degrees of twist at either end of the aim animation")
    float aimRange = 90.0f;

    HYP_FIELD(Property = "TurnLeftAnimation", Serialize, Editor, Title = "Turn Left Animation", Description = "One step turning the feet Turn Step Angle to the left")
    Name turnLeftAnimation = NAME("TurnLeft");

    HYP_FIELD(Property = "TurnRightAnimation", Serialize, Editor, Title = "Turn Right Animation", Description = "One step turning the feet Turn Step Angle to the right")
    Name turnRightAnimation = NAME("TurnRight");

    HYP_FIELD(Property = "TurnStepAngle", Serialize, Editor, Title = "Turn Step Angle", Description = "Degrees the turn step animations turn the feet; a step covers the twist it catches up, within a quarter either way of this")
    float turnStepAngle = 50.0f;

    HYP_FIELD(Property = "TurnStepStartAngle", Serialize, Editor, Title = "Turn Step Start Angle", Description = "How far the view can twist the upper body away from the feet before they step round")
    float turnStepStartAngle = 40.0f;

    HYP_FIELD(Property = "TurnLeftSmallAnimation", Serialize, Editor, Title = "Turn Left Small Animation", Description = "Short settling step turning the feet Turn Step Small Angle to the left")
    Name turnLeftSmallAnimation = NAME("TurnLeftSmall");

    HYP_FIELD(Property = "TurnRightSmallAnimation", Serialize, Editor, Title = "Turn Right Small Animation", Description = "Short settling step turning the feet Turn Step Small Angle to the right")
    Name turnRightSmallAnimation = NAME("TurnRightSmall");

    HYP_FIELD(Property = "TurnStepSmallAngle", Serialize, Editor, Title = "Turn Step Small Angle", Description = "Degrees the small turn step animations turn the feet")
    float turnStepSmallAngle = 25.0f;

    HYP_FIELD(Property = "SettleAngle", Serialize, Editor, Title = "Settle Angle", Description = "Once the view stops turning, any twist beyond this many degrees is squared up with a settling step")
    float settleAngle = 15.0f;

    HYP_FIELD(Property = "UpperBodyBone", Serialize, Editor, Title = "Upper Body Bone", Description = "Turn steps only move the legs and hips; this bone and everything above it keep following the view")
    Name upperBodyBone = NAME("spine_01");

    HYP_FIELD(Property = "WalkReferenceSpeed", Serialize, Editor, Title = "Walk Reference Speed", Description = "Speed in m/s the walk animation was authored for")
    float walkReferenceSpeed = 4.0f;

    HYP_FIELD(Property = "RunReferenceSpeed", Serialize, Editor, Title = "Run Reference Speed", Description = "Speed in m/s the run animation was authored for")
    float runReferenceSpeed = 7.5f;
};

HYP_STRUCT(Component,
    Label = "Character Model Component",
    Editor = true)
struct CharacterModelComponent
{
    HYP_STRUCT_BODY(CharacterModelComponent);

    HYP_FIELD(Property = "AlignToCapsule", Serialize, Editor, Title = "Align To Capsule", Description = "Should the models origin be at the bottom of the parent's character controller capsule?")
    bool alignToCapsule = true;

    HYP_FIELD(Property = "ModelOffset", Serialize, Editor, Title = "Model Offset", Description = "Offset applied after aligning to the capsule")
    Vec3f modelOffset = Vec3f::Zero();

    HYP_FIELD(Property = "FacingMode", Serialize, Editor, Title = "Facing Mode")
    CharacterFacingMode facingMode = CharacterFacingMode::MovementDirection;

    HYP_FIELD(Property = "TurnSharpness", Serialize, Editor, Title = "Turn Sharpness", Description = "How quickly the model turns toward its facing direction")
    float turnSharpness = 12.0f;

    HYP_FIELD(Property = "ForwardYawOffset", Serialize, Editor, Title = "Forward Yaw Offset")
    float forwardYawOffset = 0.0f;

    HYP_FIELD(Property = "Animations", Serialize, Editor)
    CharacterModelAnimations animations;

    HYP_FIELD(Property = "SpeedSmoothing", Serialize, Editor, Title = "Speed Smoothing", Description = "Seconds for the blend to catch up. (Higher = softer)")
    float speedSmoothing = 0.15f;

    HYP_FIELD(Transient)
    Vec3f previousTranslation;

    HYP_FIELD(Transient)
    bool hasPreviousTranslation = false;

    HYP_FIELD(Transient)
    float smoothedSpeed = 0.0f;

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

    // Took off with upward speed (plays Jump), as opposed to walking off a ledge (straight into Fall)
    HYP_FIELD(Transient)
    bool isJumping = false;

    HYP_FIELD(Transient)
    float airTime = 0.0f;

    // Time since touchdown; negative while no landing is playing
    HYP_FIELD(Transient)
    float landTime = -1.0f;

    HYP_FIELD(Transient)
    float verticalSpeed = 0.0f;

    // Yaw of the feet and hips, which the model entity is turned to. While standing it lags facingYaw and the upper body
    // twists to make up the difference; the feet only turn in whole steps
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

    // Radians the current step turns the feet
    HYP_FIELD(Transient)
    float turnStepSize = 0.0f;

    // Whether the current step plays the small (settling) clips
    HYP_FIELD(Transient)
    bool isTurnStepSmall = false;

    // How long the view has been holding still, for settling steps
    HYP_FIELD(Transient)
    float viewStillTime = 0.0f;

    HYP_FIELD(Transient)
    float lastFacingYaw = 0.0f;

    // 0..1 time through the current step, and the eased progress the feet and clip follow
    HYP_FIELD(Transient)
    float turnStepTime = 0.0f;

    HYP_FIELD(Transient)
    float turnStepProgress = 0.0f;

    HYP_FIELD(Transient)
    float turnStepWeight = 0.0f;
};

} // namespace Hyperion
