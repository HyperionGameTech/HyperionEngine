/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/CharacterModelSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/ThirdPersonCamera.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>

#include <Framework/GameState.hpp>

#include <cmath>

#include <CharacterModelSystem.generated.inl>

namespace Hyperion {

struct LocomotionClips
{
    uint32 idleIndex = ~0u;
    uint32 walkIndex = ~0u;
    uint32 runIndex = ~0u;
    uint32 jumpWindupIndex = ~0u;
    uint32 jumpIndex = ~0u;
    uint32 fallIndex = ~0u;
    uint32 landIndex = ~0u;

    float idleLength = 0.0f;
    float walkLength = 0.0f;
    float runLength = 0.0f;
    float jumpWindupLength = 0.0f;
    float jumpLength = 0.0f;
    float fallLength = 0.0f;
    float landLength = 0.0f;

    float aimRange = 0.0f;
    Name aimTwistRootBone;
    Name aimTwistEndBone;

    uint32 turnLeftIndex = ~0u;
    uint32 turnRightIndex = ~0u;
    uint32 turnLeftSmallIndex = ~0u;
    uint32 turnRightSmallIndex = ~0u;
    float turnLeftSmallLength = 0.0f;
    float turnRightSmallLength = 0.0f;
    float turnStepSmallAngle = 0.0f;
    float settleAngle = 0.0f;

    bool HasSmallTurnSteps() const
    {
        return turnLeftSmallIndex != ~0u && turnRightSmallIndex != ~0u && turnStepSmallAngle > 0.0f;
    }

    float turnLeftLength = 0.0f;
    float turnRightLength = 0.0f;
    float turnStepAngle = 0.0f;
    float turnStepStartAngle = 0.0f;

    bool CanStandAndAim() const
    {
        return aimTwistRootBone.IsValid() && aimTwistEndBone.IsValid() && aimRange > 0.0f
            && turnLeftIndex != ~0u && turnRightIndex != ~0u && turnStepAngle > 0.0f;
    }

    float walkReferenceSpeed = 0.0f;
    float runReferenceSpeed = 0.0f;

    bool IsValid() const
    {
        return walkIndex != ~0u;
    }
};

struct LocomotionAnimatedEntity
{
    AnimationComponent* animationComponent = nullptr;
    const Skeleton* skeleton = nullptr;
};

namespace /* Constants */ {

static constexpr float MaxTrackedSpeed = 50.0f;
static constexpr float IdleSpeedThreshold = 0.3f;

static constexpr float BackwardSpeedFraction = 0.5f;

static constexpr float JumpTakeoffSpeed = 1.0f;

static constexpr float FallGraceTime = 0.12f;

static constexpr float RemoteAirborneVerticalSpeed = 1.5f;

static constexpr float FallBlendInTime = 0.2f;

static constexpr float JumpBlendInTime = 0.25f;

static constexpr float JumpWindupHoldTime = 0.1f;

static constexpr float MovingLandFraction = 0.35f;

static constexpr float StandingSpeedFraction = 0.3f;

static constexpr float BodyCatchUpSharpness = 14.0f;

// A step plays over its clip's length, sped up by this factor when the view has already swung past the aim range
static constexpr float TurnStepFastSpeed = 1.4f;

static constexpr float TurnStepBlendInTime = 0.08f;
static constexpr float TurnStepBlendOutTime = 0.12f;

// A step cut short by starting to move gets out of the way quickly
static constexpr float TurnStepInterruptBlendOutTime = 0.04f;

// How tightly the upper body's twist follows the view (1/s); about a tenth of a second behind
static constexpr float AimTwistSharpness = 10.0f;


// The view counts as settling below this turn rate (radians/s); the settling step follows almost at once
static constexpr float SettleMaxViewTurnRate = MathUtil::DegToRad(30.0f);
static constexpr float SettleViewStillTime = 0.05f;

} // namespace 

namespace /* Helpers */ {

Entity* FindParentCharacterEntity(const Entity& entity)
{
    for (Node* parentNode = entity.GetParent(); parentNode != nullptr; parentNode = parentNode->GetParent())
    {
        if (parentNode->IsA<Entity>() && static_cast<Entity*>(parentNode)->HasComponent<CharacterControllerComponent>())
        {
            return static_cast<Entity*>(parentNode);
        }
    }

    return nullptr;
}

Vec3f GetCharacterViewDirection(const Entity& characterEntity, const CharacterControllerComponent& characterController)
{
    // Read the camera directly so the model turns on the same frame the view does
    for (const Handle<Node>& child : characterEntity.GetChildren())
    {
        if (const Handle<Camera>& camera = DynamicCast<Camera>(child); camera.IsValid())
        {
            if (const ThirdPersonCameraController* thirdPersonController = DynamicCast<ThirdPersonCameraController>(camera->GetCameraController().Get()))
            {
                return thirdPersonController->GetViewDirection();
            }

            return camera->GetDirection();
        }
    }

    return characterController.viewDirection;
}

float GetCapsuleFeetOffset(const CharacterControllerComponent& characterController, bool isPlaying)
{
    const CapsulePhysicsShape* capsuleShape = DynamicCast<CapsulePhysicsShape>(characterController.shape.Get());

    if (!capsuleShape)
    {
        return 0.0f;
    }

    const float capsuleHalfHeight = capsuleShape->GetHeight() * 0.5f + capsuleShape->GetRadius();

    // In edit mode the entity sits at the capsule center, while playing MoveCharacter lifts it by the height offset
    return -(capsuleHalfHeight + (isPlaying ? SceneHelpers::GetCapsuleHeightOffset(characterController) : 0.0f));
}

uint32 FindAnimationIndex(const Skeleton& skeleton, Name animationName)
{
    if (!animationName.IsValid())
    {
        return ~0u;
    }

    const ANSIString wantedName(animationName.LookupString());
    const ANSIString uniquifiedPrefix = wantedName + "_";

    const Array<Handle<Animation>>& animations = skeleton.GetAnimations();

    for (uint32 animationIndex = 0; animationIndex < animations.Size(); ++animationIndex)
    {
        if (!animations[animationIndex].IsValid())
        {
            continue;
        }

        const ANSIString candidateName(animations[animationIndex]->GetName().LookupString());

        // The asset registry renames duplicates to Name_N, so accept those too
        if (candidateName == wantedName || candidateName.StartsWith(uniquifiedPrefix))
        {
            return animationIndex;
        }
    }

    return ~0u;
}

float GetAnimationLength(const Skeleton& skeleton, uint32 animationIndex)
{
    const Animation* animation = skeleton.GetAnimation(animationIndex).Get();

    return animation != nullptr ? animation->GetLength() : 0.0f;
}

void FindLocomotionClips(const CharacterModelAnimations& animations, const Skeleton& skeleton, LocomotionClips& outClips)
{
    LocomotionClips& clips = outClips;

    clips = {};

    clips.idleIndex = FindAnimationIndex(skeleton, animations.idleAnimation);
    clips.walkIndex = FindAnimationIndex(skeleton, animations.walkAnimation);
    clips.runIndex = FindAnimationIndex(skeleton, animations.runAnimation);

    if (clips.walkIndex == ~0u)
    {
        clips.walkIndex = clips.runIndex;
    }

    if (clips.runIndex == ~0u)
    {
        clips.runIndex = clips.walkIndex;
    }

    clips.jumpWindupIndex = FindAnimationIndex(skeleton, animations.jumpWindupAnimation);
    clips.jumpIndex = FindAnimationIndex(skeleton, animations.jumpAnimation);
    clips.fallIndex = FindAnimationIndex(skeleton, animations.fallAnimation);
    clips.landIndex = FindAnimationIndex(skeleton, animations.landAnimation);

    clips.idleLength = GetAnimationLength(skeleton, clips.idleIndex);
    clips.walkLength = GetAnimationLength(skeleton, clips.walkIndex);
    clips.runLength = GetAnimationLength(skeleton, clips.runIndex);
    clips.jumpWindupLength = GetAnimationLength(skeleton, clips.jumpWindupIndex);
    clips.jumpLength = GetAnimationLength(skeleton, clips.jumpIndex);
    clips.fallLength = GetAnimationLength(skeleton, clips.fallIndex);
    clips.landLength = GetAnimationLength(skeleton, clips.landIndex);

    clips.aimRange = MathUtil::DegToRad(animations.aimRange);
    clips.aimTwistRootBone = animations.aimTwistRootBone;
    clips.aimTwistEndBone = animations.aimTwistEndBone;

    clips.turnLeftIndex = FindAnimationIndex(skeleton, animations.turnLeftAnimation);
    clips.turnRightIndex = FindAnimationIndex(skeleton, animations.turnRightAnimation);
    clips.turnLeftLength = GetAnimationLength(skeleton, clips.turnLeftIndex);
    clips.turnRightLength = GetAnimationLength(skeleton, clips.turnRightIndex);
    clips.turnLeftSmallIndex = FindAnimationIndex(skeleton, animations.turnLeftSmallAnimation);
    clips.turnRightSmallIndex = FindAnimationIndex(skeleton, animations.turnRightSmallAnimation);
    clips.turnLeftSmallLength = GetAnimationLength(skeleton, clips.turnLeftSmallIndex);
    clips.turnRightSmallLength = GetAnimationLength(skeleton, clips.turnRightSmallIndex);
    clips.turnStepSmallAngle = MathUtil::DegToRad(animations.turnStepSmallAngle);
    clips.settleAngle = MathUtil::DegToRad(animations.settleAngle);
    clips.turnStepAngle = MathUtil::DegToRad(animations.turnStepAngle);
    clips.turnStepStartAngle = MathUtil::DegToRad(animations.turnStepStartAngle);

    // Keep run strictly above walk so the walk -> run blend never divides by zero
    clips.walkReferenceSpeed = MathUtil::Max(animations.walkReferenceSpeed, 0.01f);
    clips.runReferenceSpeed = MathUtil::Max(animations.runReferenceSpeed, clips.walkReferenceSpeed + 0.01f);
}

float SmoothStep(float value)
{
    value = MathUtil::Clamp(value, 0.0f, 1.0f);

    return value * value * (3.0f - 2.0f * value);
}

float WrapTime(float time, float length)
{
    if (length <= 0.0f)
    {
        return 0.0f;
    }

    time = std::fmod(time, length);

    return time < 0.0f ? time + length : time;
}

/*! \brief 1D blend space over speed: idle -> walk up to the walk reference speed, walk -> run up to the run reference speed.
 *  Walk and run share one cycle phase so feet stay in step while they're mixed. */
void UpdateLocomotionPhase(CharacterModelComponent& component, const LocomotionClips& clips, bool isMovingBackward, float delta)
{
    const float runWeight = SmoothStep((component.smoothedSpeed - clips.walkReferenceSpeed) / (clips.runReferenceSpeed - clips.walkReferenceSpeed));

    // Distance covered by one full cycle of each clip at its reference speed
    const float walkStride = clips.walkReferenceSpeed * clips.walkLength;
    const float runStride = clips.runReferenceSpeed * clips.runLength;
    const float stride = MathUtil::Lerp(walkStride, runStride, runWeight);

    if (stride > 0.0f)
    {
        const float direction = isMovingBackward ? -1.0f : 1.0f;

        component.locomotionPhase = WrapTime(component.locomotionPhase + direction * component.smoothedSpeed * delta / stride, 1.0f);
    }

    component.idleTime = WrapTime(component.idleTime + delta, clips.idleLength);
}

// How far the facing direction is turned from the feet; positive when the view is to the character's right
float GetStandingTwist(const CharacterModelComponent& component)
{
    return float(std::remainder(component.facingYaw - component.bodyYaw, 2.0f * MathUtil::pi<float>));
}

/*! \brief Turns the upper body the rest of the way from the feet to the view, on top of whatever animation is playing.
 *  The twist is always the live difference, so nothing the clips do can leave the upper body pointing somewhere else. */
void ApplyAimTwist(const CharacterModelComponent& component, const LocomotionClips& clips, AnimationComponent& animationComponent)
{
    AnimationPlaybackState& playbackState = animationComponent.playbackState;

    if (!clips.CanStandAndAim())
    {
        playbackState.twistAngle = 0.0f;

        return;
    }

    playbackState.twistAngle = MathUtil::Clamp(component.aimTwist, -clips.aimRange, clips.aimRange);
    playbackState.twistRootBone = clips.aimTwistRootBone;
    playbackState.twistEndBone = clips.aimTwistEndBone;
}

void ApplyLocomotionPose(const CharacterModelComponent& component, const LocomotionClips& clips, AnimationComponent& animationComponent)
{
    const float speed = component.smoothedSpeed;

    AnimationPlaybackState& playbackState = animationComponent.playbackState;
    playbackState.status = AnimationPlaybackStatus::PLAYING;
    playbackState.loopMode = AnimationLoopMode::REPEAT;

    // Times are driven from here, so AnimationSystem shouldn't advance them
    playbackState.speed = 0.0f;
    playbackState.layerExcludedBone = Name::Invalid();

    const float walkTime = component.locomotionPhase * clips.walkLength;

    if (speed < clips.walkReferenceSpeed && clips.CanStandAndAim())
    {
        // Idle underneath; the upper body's turn toward the view is added on top by the twist (see ApplyAimTwist)
        if (clips.idleIndex != ~0u)
        {
            playbackState.animationIndex = clips.idleIndex;
            playbackState.currentTime = component.idleTime;
        }
        else
        {
            playbackState.animationIndex = clips.walkIndex;
            playbackState.currentTime = 0.0f;
        }

        const float walkWeight = SmoothStep(speed / clips.walkReferenceSpeed);

        // A step still fading out gives way as soon as walking outweighs it
        if (component.turnStepWeight > 0.001f && component.turnStepWeight >= walkWeight)
        {
            if (component.isTurnStepSmall)
            {
                playbackState.layerAnimationIndex = component.isTurnStepLeft ? clips.turnLeftSmallIndex : clips.turnRightSmallIndex;
                playbackState.layerTime = component.turnStepProgress * (component.isTurnStepLeft ? clips.turnLeftSmallLength : clips.turnRightSmallLength);
            }
            else
            {
                playbackState.layerAnimationIndex = component.isTurnStepLeft ? clips.turnLeftIndex : clips.turnRightIndex;
                playbackState.layerTime = component.turnStepProgress * (component.isTurnStepLeft ? clips.turnLeftLength : clips.turnRightLength);
            }

            playbackState.layerWeight = component.turnStepWeight;
        }
        else
        {
            playbackState.layerAnimationIndex = clips.walkIndex;
            playbackState.layerTime = walkTime;
            playbackState.layerWeight = walkWeight;
        }
    }
    else if (speed < clips.walkReferenceSpeed)
    {
        if (clips.idleIndex != ~0u)
        {
            playbackState.animationIndex = clips.idleIndex;
            playbackState.currentTime = component.idleTime;
        }
        else
        {
            playbackState.animationIndex = clips.walkIndex;
            playbackState.currentTime = 0.0f;
        }

        playbackState.layerAnimationIndex = clips.walkIndex;
        playbackState.layerTime = walkTime;
        playbackState.layerWeight = SmoothStep(speed / clips.walkReferenceSpeed);
    }
    else
    {
        playbackState.animationIndex = clips.walkIndex;
        playbackState.currentTime = walkTime;

        playbackState.layerAnimationIndex = clips.runIndex;
        playbackState.layerTime = component.locomotionPhase * clips.runLength;
        playbackState.layerWeight = SmoothStep((speed - clips.walkReferenceSpeed) / (clips.runReferenceSpeed - clips.walkReferenceSpeed));
    }
}

void UpdateBodyYaw(CharacterModelComponent& component, const LocomotionClips* clips, float delta)
{
    if (!component.hasBodyYaw)
    {
        component.bodyYaw = component.facingYaw;
        component.lastFacingYaw = component.facingYaw;
        component.aimTwist = 0.0f;
        component.hasBodyYaw = true;
    }

    const bool isStanding = clips != nullptr && clips->CanStandAndAim()
        && !component.isAirborne && component.landTime < 0.0f
        && component.smoothedSpeed < clips->walkReferenceSpeed * StandingSpeedFraction;

    if (!isStanding)
    {
        component.isTurnStepping = false;
        component.viewStillTime = 0.0f;

        const float catchUpAlpha = MathUtil::Clamp(1.0f - MathUtil::Exp(-BodyCatchUpSharpness * delta), 0.0f, 1.0f);

        component.bodyYaw += GetStandingTwist(component) * catchUpAlpha;
    }
    else
    {
        const float twist = GetStandingTwist(component);
        const float absTwist = MathUtil::Abs(twist);
        const float viewTurnRate = delta > 0.0f
            ? MathUtil::Abs(float(std::remainder(component.facingYaw - component.lastFacingYaw, 2.0f * MathUtil::pi<float>))) / delta
            : 0.0f;

        component.viewStillTime = viewTurnRate < SettleMaxViewTurnRate ? component.viewStillTime + delta : 0.0f;

        const bool isFinishingStep = (component.turnStepWeight > 0.5f);

        const bool shouldTurn = absTwist > clips->turnStepStartAngle;
        const bool shouldSettle = clips->HasSmallTurnSteps() && absTwist > clips->settleAngle
            && (component.viewStillTime > SettleViewStillTime || isFinishingStep);

        if (!component.isTurnStepping && (shouldTurn || shouldSettle))
        {
            component.isTurnStepping = true;
            component.isTurnStepLeft = twist < 0.0f;
            component.turnStepFromYaw = component.bodyYaw;

            component.isTurnStepSmall = (clips->HasSmallTurnSteps() && absTwist < (clips->turnStepSmallAngle + clips->turnStepAngle) * 0.5f);
            
            float stepAngle = clips->turnStepAngle;
            if (component.isTurnStepSmall)
            {
                stepAngle = clips->turnStepSmallAngle;
            }

            // Full steps turn exactly their clip's angle, so the recorded feet stay planted; the smaller everyday steps
            // catch up the actual twist, staying close to their clip's angle
            component.turnStepSize = component.isTurnStepSmall ? MathUtil::Clamp(absTwist, stepAngle * 0.5f, stepAngle * 1.25f) : stepAngle;
            component.turnStepTime = 0.0f;
            component.turnStepProgress = 0.0f;
        }

        if (component.isTurnStepping)
        {
            const float clipLength = component.isTurnStepSmall
                ? (component.isTurnStepLeft ? clips->turnLeftSmallLength : clips->turnRightSmallLength)
                : (component.isTurnStepLeft ? clips->turnLeftLength : clips->turnRightLength);

            const float duration = MathUtil::Max(clipLength, 0.1f) / (absTwist > clips->aimRange ? TurnStepFastSpeed : 1.0f);

            component.turnStepTime = MathUtil::Min(component.turnStepTime + delta / duration, 1.0f);
            // The clips are authored against a body turning at a steady rate, so the feet only stay planted if it does
            component.turnStepProgress = component.turnStepTime;

            const float direction = component.isTurnStepLeft ? -1.0f : 1.0f;

            component.bodyYaw = component.turnStepFromYaw + direction * component.turnStepSize * component.turnStepProgress;

            if (component.turnStepTime >= 1.0f)
            {
                component.isTurnStepping = false;
            }
        }
    }

    const float targetWeight = component.isTurnStepping ? 1.0f : 0.0f;
    const float blendTime = targetWeight > component.turnStepWeight ? TurnStepBlendInTime
        : isStanding                                                 ? TurnStepBlendOutTime
                                                                     : TurnStepInterruptBlendOutTime;

    component.turnStepWeight = MathUtil::Lerp(component.turnStepWeight, targetWeight, MathUtil::Clamp(1.0f - MathUtil::Exp(-delta / blendTime), 0.0f, 1.0f));

    // Ease the upper body's twist toward the gap between the feet and the view rather than locking it there
    const float twistAlpha = MathUtil::Clamp(1.0f - MathUtil::Exp(-AimTwistSharpness * delta), 0.0f, 1.0f);

    component.aimTwist += float(std::remainder(GetStandingTwist(component) - component.aimTwist, 2.0f * MathUtil::pi<float>)) * twistAlpha;

    component.lastFacingYaw = component.facingYaw;
}

void UpdateAirborneState(CharacterModelComponent& component, bool isOnGround, float delta)
{
    if (!isOnGround)
    {
        if (!component.isAirborne)
        {
            component.isAirborne = true;
            component.isJumping = component.verticalSpeed > JumpTakeoffSpeed;
            component.airTime = 0.0f;
        }
        else
        {
            component.airTime += delta;

            if (!component.isJumping && component.airTime < FallGraceTime && component.verticalSpeed > JumpTakeoffSpeed)
            {
                component.isJumping = true;
            }
        }

        return;
    }

    if (component.isAirborne)
    {
        component.isAirborne = false;
        component.isJumpWoundUp = false;

        // A step down that never left the locomotion pose doesn't need a landing
        component.landTime = (component.isJumping || component.airTime > FallGraceTime) ? 0.0f : -1.0f;
    }
    else if (component.landTime >= 0.0f)
    {
        component.landTime += delta;
    }
}

void UpdateJumpWindup(CharacterModelComponent& component, const CharacterControllerComponent* characterController, float delta)
{
    const float windupTime = characterController != nullptr ? characterController->jump.windupTime : 0.0f;

    if (windupTime > 0.0f && characterController->jumpWindupRemaining > 0.0f)
    {
        component.jumpWindup = MathUtil::Clamp(1.0f - characterController->jumpWindupRemaining / windupTime, 0.0f, 1.0f);
        component.isJumpWoundUp = true;
        component.jumpWindupHoldTime = 0.0f;

        return;
    }

    component.jumpWindup = -1.0f;

    if (!component.isJumpWoundUp)
    {
        return;
    }

    if (component.isAirborne)
    {
        if (!component.isJumping && component.airTime >= FallGraceTime)
        {
            component.isJumpWoundUp = false;
        }

        return;
    }

    component.jumpWindupHoldTime += delta;

    if (component.jumpWindupHoldTime > JumpWindupHoldTime)
    {
        component.isJumpWoundUp = false;
    }
}

uint32 GetDominantLocomotionClip(const CharacterModelComponent& component, const LocomotionClips& clips, float& outTime)
{
    const float speed = component.smoothedSpeed;

    if (speed < clips.walkReferenceSpeed * 0.5f && clips.idleIndex != ~0u)
    {
        outTime = component.idleTime;

        return clips.idleIndex;
    }

    if (speed < (clips.walkReferenceSpeed + clips.runReferenceSpeed) * 0.5f)
    {
        outTime = component.locomotionPhase * clips.walkLength;

        return clips.walkIndex;
    }

    outTime = component.locomotionPhase * clips.runLength;

    return clips.runIndex;
}

bool ApplyAirbornePose(CharacterModelComponent& component, const LocomotionClips& clips, AnimationComponent& animationComponent)
{
    AnimationPlaybackState& playbackState = animationComponent.playbackState;
    playbackState.layerExcludedBone = Name::Invalid();

    if (!component.isAirborne && component.isJumpWoundUp && clips.jumpWindupIndex != ~0u)
    {
        const float windup = component.jumpWindup >= 0.0f ? component.jumpWindup : 1.0f;
        float locomotionTime = 0.0f;

        playbackState.animationIndex = clips.jumpWindupIndex;
        playbackState.currentTime = windup * clips.jumpWindupLength;
        playbackState.layerAnimationIndex = GetDominantLocomotionClip(component, clips, locomotionTime);
        playbackState.layerTime = locomotionTime;
        playbackState.layerWeight = 1.0f - SmoothStep(windup);
    }
    else if (component.isAirborne)
    {
        if (!component.isJumping && component.airTime < FallGraceTime)
        {
            return false;
        }

        if (component.isJumping && clips.jumpIndex != ~0u)
        {
            playbackState.animationIndex = clips.jumpIndex;
            playbackState.currentTime = MathUtil::Min(component.airTime, clips.jumpLength);

            if (component.airTime < JumpBlendInTime && !component.isJumpWoundUp)
            {
                float locomotionTime = 0.0f;

                playbackState.layerAnimationIndex = GetDominantLocomotionClip(component, clips, locomotionTime);
                playbackState.layerTime = locomotionTime;
                playbackState.layerWeight = 1.0f - SmoothStep(component.airTime / JumpBlendInTime);
            }
            else
            {
                playbackState.layerAnimationIndex = clips.fallIndex;
                playbackState.layerTime = WrapTime(MathUtil::Max(component.airTime - clips.jumpLength, 0.0f), clips.fallLength);
                playbackState.layerWeight = clips.fallIndex != ~0u
                    ? SmoothStep((component.airTime - clips.jumpLength * 0.6f) / MathUtil::Max(clips.jumpLength * 0.4f, 0.001f))
                    : 0.0f;
            }
        }
        else if (clips.fallIndex != ~0u)
        {
            float locomotionTime = 0.0f;

            playbackState.animationIndex = clips.fallIndex;
            playbackState.currentTime = WrapTime(component.airTime, clips.fallLength);
            playbackState.layerAnimationIndex = GetDominantLocomotionClip(component, clips, locomotionTime);
            playbackState.layerTime = locomotionTime;
            playbackState.layerWeight = 1.0f - SmoothStep((component.airTime - FallGraceTime) / FallBlendInTime);
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (component.landTime < 0.0f || clips.landIndex == ~0u)
        {
            return false;
        }

        const bool isMoving = component.smoothedSpeed > clips.walkReferenceSpeed * 0.5f;
        const float landDuration = clips.landLength * (isMoving ? MovingLandFraction : 1.0f);

        if (component.landTime >= landDuration)
        {
            component.landTime = -1.0f;

            return false;
        }

        float locomotionTime = 0.0f;

        playbackState.animationIndex = clips.landIndex;
        playbackState.currentTime = component.landTime;
        playbackState.layerAnimationIndex = GetDominantLocomotionClip(component, clips, locomotionTime);
        playbackState.layerTime = locomotionTime;
        playbackState.layerWeight = SmoothStep(component.landTime / MathUtil::Max(landDuration, 0.001f));
    }

    playbackState.status = AnimationPlaybackStatus::PLAYING;
    playbackState.loopMode = AnimationLoopMode::REPEAT;
    playbackState.speed = 0.0f;

    return true;
}

void UpdateFacing(Entity& entity, CharacterModelComponent& component, const Vec3f& facingDirection, float delta)
{
    if (!component.hasFacingYaw)
    {
        const Vec3f currentForward = entity.GetWorldRotation().Inverse().RotateVector(Vec3f::UnitZ());

        component.facingYaw = std::atan2(currentForward.x, currentForward.z) - MathUtil::DegToRad(component.forwardYawOffset);
        component.hasFacingYaw = true;
    }

    if (Vec3f(facingDirection.x, 0.0f, facingDirection.z).LengthSquared() > 0.0001f)
    {
        const float targetYaw = std::atan2(facingDirection.x, facingDirection.z);
        const float yawDifference = float(std::remainder(targetYaw - component.facingYaw, 2.0f * MathUtil::pi<float>));
        const float turnAlpha = 1.0f - MathUtil::Exp(-MathUtil::Max(component.turnSharpness, 0.0f) * delta);

        component.facingYaw += yawDifference * MathUtil::Clamp(turnAlpha, 0.0f, 1.0f);
    }
}

void ApplyBodyRotation(Entity& entity, const CharacterModelComponent& component)
{
    const float modelYaw = component.bodyYaw + MathUtil::DegToRad(component.forwardYawOffset);

    // Transforms store the inverse rotation, so negate the yaw to turn +Z toward the facing direction
    entity.SetWorldRotation(Quat4f(Vec3f::UnitY(), -modelYaw), TransformChangeType::Simulation);
}

} // namespace

bool CharacterModelSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | SceneFlags::EDITOR | ExpectedFlags)) == ExpectedFlags;
}

void CharacterModelSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    const GameState& gameState = GetWorld()->GetGameState();

    const bool isPlaying = !gameState.IsStopped();
    const bool isSimulating = gameState.IsSimulating();

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<CharacterModelComponent>().GetScopedView(GetComponentInfos()))
        {
            const Entity* characterEntity = FindParentCharacterEntity(*entity);

            const CharacterControllerComponent* characterController = characterEntity != nullptr
                ? characterEntity->TryGetComponent<CharacterControllerComponent>()
                : nullptr;

            if (component.alignToCapsule && characterController != nullptr)
            {
                const Vec3f modelTranslation = Vec3f(0.0f, GetCapsuleFeetOffset(*characterController, isPlaying), 0.0f) + component.modelOffset;

                if (entity->GetLocalTranslation() != modelTranslation)
                {
                    entity->SetLocalTranslation(modelTranslation, TransformChangeType::Simulation);
                }
            }

            if (!isSimulating)
            {
                component.hasPreviousTranslation = false;
                component.smoothedSpeed = 0.0f;
                component.isAirborne = false;
                component.landTime = -1.0f;
                component.hasBodyYaw = false;
                component.isTurnStepping = false;
                component.turnStepWeight = 0.0f;

                continue;
            }

            const Vec3f worldTranslation = entity->GetWorldTranslation();

            Vec3f horizontalVelocity = Vec3f::Zero();
            float verticalSpeed = 0.0f;

            if (component.hasPreviousTranslation && delta > 0.0f)
            {
                const Vec3f displacement = worldTranslation - component.previousTranslation;

                horizontalVelocity = Vec3f(displacement.x, 0.0f, displacement.z) * (1.0f / delta);
                verticalSpeed = displacement.y / delta;

                if (horizontalVelocity.Length() > MaxTrackedSpeed || MathUtil::Abs(verticalSpeed) > MaxTrackedSpeed)
                {
                    // teleport or reconciliation snap
                    horizontalVelocity = Vec3f::Zero();
                    verticalSpeed = 0.0f;
                }
            }

            component.verticalSpeed = verticalSpeed;

            component.previousTranslation = worldTranslation;
            component.hasPreviousTranslation = true;

            const float speedAlpha = component.speedSmoothing > 0.0f
                ? 1.0f - MathUtil::Exp(-delta / component.speedSmoothing)
                : 1.0f;

            component.smoothedSpeed = MathUtil::Lerp(component.smoothedSpeed, horizontalVelocity.Length(), MathUtil::Clamp(speedAlpha, 0.0f, 1.0f));

            const Vec3f movementDirection = horizontalVelocity.Length() >= IdleSpeedThreshold
                ? horizontalVelocity
                : Vec3f::Zero();

            const PlayerComponent* playerComponent = characterEntity != nullptr ? characterEntity->TryGetComponent<PlayerComponent>() : nullptr;
            const bool isRemotePlayer = playerComponent != nullptr && !playerComponent->IsLocalPlayer();

            if (characterController != nullptr)
            {
                const bool isOnGround = isRemotePlayer
                    ? MathUtil::Abs(verticalSpeed) < RemoteAirborneVerticalSpeed
                    : characterController->isOnGround;

                UpdateAirborneState(component, isOnGround, delta);
                UpdateJumpWindup(component, isRemotePlayer ? nullptr : characterController, delta);
            }

            switch (component.facingMode)
            {
            case CharacterFacingMode::MovementDirection:
                UpdateFacing(*entity, component, movementDirection, delta);

                break;
            case CharacterFacingMode::ViewDirection:
            {
                // Remote players' cameras aren't driven by anyone on this machine, so follow their movement instead
                if (characterController != nullptr && !isRemotePlayer)
                {
                    UpdateFacing(*entity, component, GetCharacterViewDirection(*characterEntity, *characterController), delta);
                }
                else
                {
                    UpdateFacing(*entity, component, movementDirection, delta);
                }

                break;
            }
            default:
                break;
            }

            const Vec3f facingForward = Vec3f(std::sin(component.facingYaw), 0.0f, std::cos(component.facingYaw));
            const bool isMovingBackward = movementDirection.Dot(facingForward) < -BackwardSpeedFraction * movementDirection.Length();

            Array<LocomotionAnimatedEntity, SceneTempAllocator> animatedEntities;

            for (Node* descendant : entity->GetDescendants())
            {
                if (!descendant->IsA<Entity>())
                {
                    continue;
                }

                Entity* descendantEntity = static_cast<Entity*>(descendant);

                AnimationComponent* animationComponent = descendantEntity->TryGetComponent<AnimationComponent>();
                const MeshComponent* meshComponent = descendantEntity->TryGetComponent<MeshComponent>();

                if (!animationComponent || !meshComponent || !meshComponent->skeleton.IsValid())
                {
                    continue;
                }

                animatedEntities.PushBack(LocomotionAnimatedEntity { animationComponent, meshComponent->skeleton.Get() });
            }

            // Submeshes of one model share a skeleton; advance the phase once per character, from the first one
            LocomotionClips phaseClips;
            
            if (animatedEntities.Any())
            {
                FindLocomotionClips(component.animations, *animatedEntities[0].skeleton, phaseClips);
            }

            // With no facing mode the model keeps whatever rotation it has
            if (component.hasFacingYaw)
            {
                UpdateBodyYaw(component, phaseClips.IsValid() ? &phaseClips : nullptr, delta);
                ApplyBodyRotation(*entity, component);
            }

            if (!phaseClips.IsValid())
            {
                continue;
            }

            UpdateLocomotionPhase(component, phaseClips, isMovingBackward, delta);

            for (const LocomotionAnimatedEntity& animatedEntity : animatedEntities)
            {
                Optional<LocomotionClips> clipsOpt;
                LocomotionClips* pClips = nullptr;
                
                if (animatedEntity.skeleton == animatedEntities[0].skeleton)
                {
                    pClips = &phaseClips;
                }
                else
                {
                    FindLocomotionClips(component.animations, *animatedEntity.skeleton, clipsOpt.Emplace());
                    pClips = &clipsOpt.GetUnchecked();
                }

                if (pClips->IsValid() && !ApplyAirbornePose(component, *pClips, *animatedEntity.animationComponent))
                {
                    ApplyLocomotionPose(component, *pClips, *animatedEntity.animationComponent);
                }

                ApplyAimTwist(component, *pClips, *animatedEntity.animationComponent);
            }
        }
    }
}

} // namespace Hyperion
