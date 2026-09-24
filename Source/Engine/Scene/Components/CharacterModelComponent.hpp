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
    None = 0,          //!< @title="None" @description="The model keeps whatever rotation it has"
    MovementDirection, //!< @title="Movement Direction" @description="Turns toward the direction the character moves"
    ViewDirection      //!< @title="View Direction" @description="Always faces where the player's camera looks, so sideways input strafes"
};

/*! \brief Put on the entity holding a character's visual model, as a child of the entity with the CharacterControllerComponent.
 *  Keeps the model's feet on the bottom of the capsule, turns it according to its facing mode and plays locomotion animations
 *  on any animated meshes beneath it. */
HYP_STRUCT(Component,
    Label = "Character Model Component",
    Description = "Aligns a character model to its parent's character controller, turns it to face movement or the camera and plays idle/walk/run animations",
    Editor = true)
struct CharacterModelComponent
{
    HYP_STRUCT_BODY(CharacterModelComponent);

    HYP_FIELD(Property = "AlignToCapsule", Serialize, Editor, Title = "Align To Capsule", Description = "Keep the model's origin at the bottom of the parent's character controller capsule")
    bool alignToCapsule = true;

    HYP_FIELD(Property = "ModelOffset", Serialize, Editor, Title = "Model Offset", Description = "Extra offset applied after aligning to the capsule")
    Vec3f modelOffset = Vec3f::Zero();

    HYP_FIELD(Property = "FacingMode", Serialize, Editor, Title = "Facing Mode")
    CharacterFacingMode facingMode = CharacterFacingMode::MovementDirection;

    HYP_FIELD(Property = "TurnSharpness", Serialize, Editor, Title = "Turn Sharpness", Description = "How quickly the model turns toward its facing direction")
    float turnSharpness = 12.0f;

    HYP_FIELD(Property = "ForwardYawOffset", Serialize, Editor, Title = "Forward Yaw Offset", Description = "Rotation in degrees to apply if the model does not face +Z")
    float forwardYawOffset = 0.0f;

    HYP_FIELD(Property = "IdleAnimation", Serialize, Editor, Title = "Idle Animation", Description = "Played while standing still. If not found, the first frame of the walk animation is held")
    Name idleAnimation = NAME("Idle");

    HYP_FIELD(Property = "WalkAnimation", Serialize, Editor, Title = "Walk Animation")
    Name walkAnimation = NAME("Walk");

    HYP_FIELD(Property = "RunAnimation", Serialize, Editor, Title = "Run Animation")
    Name runAnimation = NAME("Run");

    HYP_FIELD(Property = "WalkReferenceSpeed", Serialize, Editor, Title = "Walk Reference Speed", Description = "Ground speed in m/s the walk animation plays at 1x")
    float walkReferenceSpeed = 4.0f;

    HYP_FIELD(Property = "RunReferenceSpeed", Serialize, Editor, Title = "Run Reference Speed", Description = "Ground speed in m/s the run animation plays at 1x")
    float runReferenceSpeed = 7.5f;

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
};

} // namespace Hyperion
