/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Animation/CharacterModelSystem.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Animation/AnimationGraph.hpp>
#include <Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Animation/HumanoidMapping.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/ThirdPersonCamera.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Physics/PhysicsShape.hpp>
#include <Physics/PhysicsWorld.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>

#include <Framework/GameState.hpp>

#include <cmath>

#include <CharacterModelSystem.generated.inl>

namespace Hyperion {

namespace {

static constexpr float MaxTrackedSpeed = 50.0f;
static constexpr float IdleSpeedThreshold = 0.3f;

static constexpr float JumpTakeoffSpeed = 1.0f;

static constexpr float FallGraceTime = 0.12f;

static constexpr float RemoteAirborneVerticalSpeed = 1.5f;

static constexpr float JumpWindupHoldTime = 0.1f;

// Leaving the ground faster than this (m/s) is a running jump, which has no windup
static constexpr float RunningJumpSpeed = 3.0f;

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

// what the character is doing this frame, written to its graph as parameters
struct CharacterMotionSignals
{
    float speed = 0.0f;
    float moveAngle = 0.0f;
    float windup = 0.0f;
    bool isAirborne = false;
    bool isJumping = false;
    bool isRunningJump = false;
    float airTime = 0.0f;
    float turnStep = 0.0f;
    float turnStepKind = 0.0f;
    float turnStepProgress = 0.0f;
    float aimTwist = 0.0f;

    // for motion matching: the body's velocity and the one it wants (x to its right, y forward), and where it wants to face
    Vec2f velocity;
    Vec2f desiredVelocity;
    float desiredFacing = 0.0f;
    float trajectoryAcceleration = 0.0f;
    float trajectoryTurnSharpness = 0.0f;
};

// the turn step clips in the character's graph, found by node name, which time the body's turn
struct TurnStepClips
{
    float leftLength = 0.0f;
    float rightLength = 0.0f;
    float leftSmallLength = 0.0f;
    float rightSmallLength = 0.0f;

    bool HasFullSteps() const
    {
        return leftLength > 0.0f && rightLength > 0.0f;
    }

    bool HasSmallSteps() const
    {
        return leftSmallLength > 0.0f && rightSmallLength > 0.0f;
    }
};


Entity* FindParentCharacterEntity(const Entity& entity)
{
    for (Node* parentNode = entity.GetParent(); parentNode != nullptr; parentNode = parentNode->GetParent())
    {
        if (parentNode->IsA<Entity>()
            && Cast<Entity>(parentNode)->HasComponent<CharacterControllerComponent>())
        {
            return Cast<Entity>(parentNode);
        }
    }

    return nullptr;
}

// bone-attached entities (weapons, props) keep their own animation
void CollectBodyAnimatedEntities(const Node& node, Array<Entity*>& out)
{
    for (const Handle<Node>& child : node.GetChildren())
    {
        Entity* childEntity = TryCast<Entity>(child.Get());

        if (childEntity && childEntity->HasComponent<BoneAttachmentComponent>())
        {
            continue;
        }

        if (childEntity && childEntity->HasComponent<AnimationComponent>())
        {
            out.PushBack(childEntity);
        }

        CollectBodyAnimatedEntities(*child, out);
    }
}

Vec3f GetCharacterViewDirection(
    const Entity& characterEntity,
    const CharacterControllerComponent& characterController)
{
    // Read the camera directly so the model turns on the same frame the view does
    for (const Handle<Node>& child : characterEntity.GetChildren())
    {
        if (const Handle<Camera>& camera = TryCast<Camera>(child); camera.IsValid())
        {
            if (const ThirdPersonCameraController* thirdPersonController = TryCast<ThirdPersonCameraController>(camera->GetCameraController().Get()))
            {
                return thirdPersonController->GetViewDirection();
            }

            return camera->GetDirection();
        }
    }

    return characterController.viewDirection;
}

float GetCapsuleFeetOffset(
    const CharacterControllerComponent& characterController,
    bool isPlaying)
{
    const CapsulePhysicsShape* capsuleShape = TryCast<CapsulePhysicsShape>(characterController.shape.Get());

    if (!capsuleShape)
    {
        return 0.0f;
    }

    const float capsuleHalfHeight = capsuleShape->GetHeight() * 0.5f + capsuleShape->GetRadius();

    // In edit mode the entity sits at the capsule center, while playing MoveCharacter lifts it by the height offset
    return -(capsuleHalfHeight + (isPlaying ? SceneHelpers::GetCapsuleHeightOffset(characterController) : 0.0f));
}

float GetGraphClipLength(const AnimationGraph& graph, Name nodeName)
{
    const AnimationClipNode* clipNode = TryCast<AnimationClipNode>(graph.GetNode(graph.FindNode(nodeName)));

    return clipNode && clipNode->clip.IsValid() ? clipNode->clip->GetLength() : 0.0f;
}

TurnStepClips FindTurnStepClips(const AnimationGraph& graph)
{
    TurnStepClips clips;
    clips.leftLength = GetGraphClipLength(graph, NAME("TurnLeft"));
    clips.rightLength = GetGraphClipLength(graph, NAME("TurnRight"));
    clips.leftSmallLength = GetGraphClipLength(graph, NAME("TurnLeftSmall"));
    clips.rightSmallLength = GetGraphClipLength(graph, NAME("TurnRightSmall"));

    return clips;
}

bool CanStandAndAim(const CharacterModelComponent& component, const TurnStepClips& clips)
{
    return component.aimRange > 0.0f && component.turnStepAngle > 0.0f && clips.HasFullSteps();
}

bool HasSmallTurnSteps(const CharacterModelComponent& component, const TurnStepClips& clips)
{
    return component.turnStepSmallAngle > 0.0f && clips.HasSmallSteps();
}

// How far the facing direction is turned from the feet; positive when the view is to the character's right
float GetStandingTwist(const CharacterModelComponent& component)
{
    return float(std::remainder(component.facingYaw - component.bodyYaw, 2.0f * MathUtil::pi<float>));
}

float GetTurnStepClipLength(const CharacterModelComponent& component, const TurnStepClips& clips)
{
    if (component.isTurnStepSmall)
    {
        return component.isTurnStepLeft ? clips.leftSmallLength : clips.rightSmallLength;
    }

    return component.isTurnStepLeft ? clips.leftLength : clips.rightLength;
}

void UpdateBodyYaw(CharacterModelComponent& component, const TurnStepClips& clips, float delta)
{
    if (!component.hasBodyYaw)
    {
        component.bodyYaw = component.facingYaw;
        component.lastFacingYaw = component.facingYaw;
        component.aimTwist = 0.0f;
        component.hasBodyYaw = true;
    }

    const float aimRange = MathUtil::DegToRad(component.aimRange);
    const float turnStepAngle = MathUtil::DegToRad(component.turnStepAngle);
    const float turnStepSmallAngle = MathUtil::DegToRad(component.turnStepSmallAngle);

    const bool isStanding = CanStandAndAim(component, clips)
        && !component.isAirborne
        && component.smoothedSpeed < component.walkReferenceSpeed * StandingSpeedFraction;

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

        const bool isFinishingStep = component.turnStepWeight > 0.5f;
        const bool hasSmallSteps = HasSmallTurnSteps(component, clips);

        const bool shouldTurn = absTwist > MathUtil::DegToRad(component.turnStepStartAngle);
        const bool shouldSettle = hasSmallSteps && absTwist > MathUtil::DegToRad(component.settleAngle)
            && (component.viewStillTime > SettleViewStillTime || isFinishingStep);

        if (!component.isTurnStepping && (shouldTurn || shouldSettle))
        {
            component.isTurnStepping = true;
            component.isTurnStepLeft = twist < 0.0f;
            component.turnStepFromYaw = component.bodyYaw;
            component.isTurnStepSmall = hasSmallSteps && absTwist < (turnStepSmallAngle + turnStepAngle) * 0.5f;

            // Full steps turn exactly their clip's angle, so the recorded feet stay planted; the smaller everyday steps
            // catch up the actual twist, staying close to their clip's angle
            if (component.isTurnStepSmall)
            {
                component.turnStepSize = MathUtil::Clamp(absTwist, turnStepSmallAngle * 0.5f, turnStepSmallAngle * 1.25f);
            }
            else
            {
                component.turnStepSize = turnStepAngle;
            }

            component.turnStepTime = 0.0f;
        }

        if (component.isTurnStepping)
        {
            const float duration = MathUtil::Max(GetTurnStepClipLength(component, clips), 0.1f) / (absTwist > aimRange ? TurnStepFastSpeed : 1.0f);

            // The clips are authored against a body turning at a steady rate, so the feet only stay planted if it does
            component.turnStepTime = MathUtil::Min(component.turnStepTime + delta / duration, 1.0f);

            const float direction = component.isTurnStepLeft ? -1.0f : 1.0f;

            component.bodyYaw = component.turnStepFromYaw + direction * component.turnStepSize * component.turnStepTime;

            if (component.turnStepTime >= 1.0f)
            {
                component.isTurnStepping = false;
            }
        }
    }

    const float targetWeight = component.isTurnStepping ? 1.0f : 0.0f;

    float blendTime = TurnStepInterruptBlendOutTime;

    if (targetWeight > component.turnStepWeight)
    {
        blendTime = TurnStepBlendInTime;
    }
    else if (isStanding)
    {
        blendTime = TurnStepBlendOutTime;
    }

    component.turnStepWeight = MathUtil::Lerp(component.turnStepWeight, targetWeight, MathUtil::Clamp(1.0f - MathUtil::Exp(-delta / blendTime), 0.0f, 1.0f));

    // Ease the upper body's twist toward the gap between the feet and the view rather than locking it there
    const float twistAlpha = MathUtil::Clamp(1.0f - MathUtil::Exp(-AimTwistSharpness * delta), 0.0f, 1.0f);

    component.aimTwist += (GetStandingTwist(component) - component.aimTwist) * twistAlpha;

    component.lastFacingYaw = component.facingYaw;
}

void UpdateAirborneState(
    CharacterModelComponent& component,
    bool isOnGround,
    float delta)
{
    if (!isOnGround)
    {
        if (!component.isAirborne)
        {
            component.isAirborne = true;
            // A wound-up takeoff is a jump even when the capsule hasn't moved since the last update
            component.isJumping = component.verticalSpeed > JumpTakeoffSpeed || component.isJumpWoundUp;
            component.isRunningJump = component.smoothedSpeed > RunningJumpSpeed;
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
    }
}

void UpdateJumpWindup(
    CharacterModelComponent& component,
    const CharacterControllerComponent* characterController,
    float delta)
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

CharacterMotionSignals GatherSignals(const CharacterModelComponent& component, const TurnStepClips& clips)
{
    CharacterMotionSignals signals;
    signals.speed = component.smoothedSpeed;
    signals.moveAngle = MathUtil::RadToDeg(component.moveAngle);
    signals.isAirborne = component.isAirborne;
    signals.isJumping = component.isAirborne && component.isJumping;
    signals.isRunningJump = component.isRunningJump;
    signals.airTime = component.isAirborne ? component.airTime : 0.0f;

    // a running takeoff skips the windup; a finished one holds its last frame until the feet leave the ground
    if (component.isJumpWoundUp && component.smoothedSpeed <= RunningJumpSpeed)
    {
        signals.windup = component.jumpWindup >= 0.0f ? component.jumpWindup : 1.0f;
    }

    signals.turnStep = component.turnStepWeight;
    signals.turnStepProgress = component.turnStepTime;
    signals.turnStepKind = float((component.isTurnStepSmall ? 2 : 0) + (component.isTurnStepLeft ? 0 : 1));

    if (CanStandAndAim(component, clips))
    {
        const float aimRange = MathUtil::DegToRad(component.aimRange);

        signals.aimTwist = MathUtil::Clamp(component.aimTwist, -aimRange, aimRange);
    }

    return signals;
}

void WriteSignals(const CharacterMotionSignals& signals, AnimationGraphInstance& instance)
{
    instance.SetFloat(NAME("Speed"), signals.speed);
    instance.SetFloat(NAME("MoveAngle"), signals.moveAngle);
    instance.SetFloat(NAME("Windup"), signals.windup);
    instance.SetBool(NAME("Airborne"), signals.isAirborne);
    instance.SetBool(NAME("Jumping"), signals.isJumping);
    instance.SetBool(NAME("RunningJump"), signals.isRunningJump);
    instance.SetFloat(NAME("AirTime"), signals.airTime);
    instance.SetFloat(NAME("TurnStep"), signals.turnStep);
    instance.SetFloat(NAME("TurnStepKind"), signals.turnStepKind);
    instance.SetFloat(NAME("TurnStepProgress"), signals.turnStepProgress);
    instance.SetFloat(NAME("AimTwist"), signals.aimTwist);
    instance.SetVector2(NAME("Velocity"), signals.velocity);
    instance.SetVector2(NAME("DesiredVelocity"), signals.desiredVelocity);
    instance.SetFloat(NAME("DesiredFacing"), signals.desiredFacing);
    instance.SetFloat(NAME("TrajectoryAcceleration"), signals.trajectoryAcceleration);
    instance.SetFloat(NAME("TrajectoryTurnSharpness"), signals.trajectoryTurnSharpness);
}

float GetFacingTurnAlpha(const CharacterModelComponent& component, float delta)
{
    return MathUtil::Clamp(1.0f - MathUtil::Exp(-MathUtil::Max(component.turnSharpness, 0.0f) * delta), 0.0f, 1.0f);
}

void UpdateFacing(Entity& entity, CharacterModelComponent& component, const Vec3f& facingDirection, float turnAlpha)
{
    if (!component.hasFacingYaw)
    {
        const Vec3f currentForward = entity.GetWorldRotation().RotateVector(Vec3f::UnitZ());

        component.facingYaw = std::atan2(currentForward.x, currentForward.z) - MathUtil::DegToRad(component.forwardYawOffset);
        component.hasFacingYaw = true;
    }

    if (Vec3f(facingDirection.x, 0.0f, facingDirection.z).LengthSquared() > 0.0001f)
    {
        const float targetYaw = std::atan2(facingDirection.x, facingDirection.z);
        const float yawDifference = float(std::remainder(targetYaw - component.facingYaw, 2.0f * MathUtil::pi<float>));

        component.facingYaw += yawDifference * turnAlpha;
    }
}

void ApplyBodyRotation(Entity& entity, const CharacterModelComponent& component)
{
    const float modelYaw = component.bodyYaw + MathUtil::DegToRad(component.forwardYawOffset);

    entity.SetWorldRotation(Quat4f(Vec3f::UnitY(), modelYaw), TransformChangeType::Simulation);
}

// how far the ground under a foot is above the capsule's floor; zero when nothing is in reach
float ProbeFootOffset(PhysicsWorldBase& physicsWorld, const CharacterModelComponent& component, const Vec3f& footPosition, float floorHeight)
{
    const Vec3f origin(footPosition.x, floorHeight + component.footProbeHeight, footPosition.z);

    PhysicsRayHit hit;

    if (!physicsWorld.CastRayStatic(origin, -Vec3f::UnitY(), component.footProbeHeight + component.footProbeDepth, hit))
    {
        return 0.0f;
    }

    return MathUtil::Clamp(hit.position.y - floorHeight, -component.footProbeDepth, component.footProbeHeight);
}

void UpdateFootOffsets(CharacterModelComponent& component, const Entity& meshEntity, PhysicsWorldBase* physicsWorld, float floorHeight, float delta)
{
    float leftTarget = 0.0f;
    float rightTarget = 0.0f;

    const MeshComponent* meshComponent = meshEntity.TryGetComponent<MeshComponent>();

    if (component.footIK && !component.isAirborne && physicsWorld != nullptr && meshComponent != nullptr && meshComponent->skeleton.IsValid())
    {
        const Skeleton& skeleton = *meshComponent->skeleton;
        const SkeletonBindData& bindData = skeleton.GetBindData();
        const HumanoidMapping& mapping = HumanoidSkeletonData::GetSkeletonMapping(skeleton);

        const Handle<Skeleton>& renderSkeleton = meshComponent->GetRenderSkeleton();
        const Span<const Mat4f> poseMatrices = renderSkeleton.IsValid() ? renderSkeleton->GetPoseModelMatrices() : Span<const Mat4f>();

        // only the feet's ground position matters, so last frame's pose (with its foot IK) is as good as any
        const auto probeFoot = [&](HumanoidBone bone)
        {
            const uint32 slot = bindData.FindSlot(mapping.GetBoneName(bone));
            Assert(slot != SkeletonBindData::InvalidSlot);

            const Vec3f footPosition = (meshEntity.GetWorldMatrix() * poseMatrices[slot]).ExtractTranslation();

            return ProbeFootOffset(*physicsWorld, component, footPosition, floorHeight);
        };

        if (HumanoidSkeletonData::IsHumanoid(skeleton) && poseMatrices.Size() == bindData.NumSlots())
        {
            leftTarget = probeFoot(HumanoidBone::LeftFoot);
            rightTarget = probeFoot(HumanoidBone::RightFoot);
        }
    }

    float alpha = 1.0f;

    if (component.footIKSmoothing > 0.0f)
    {
        alpha = MathUtil::Clamp(1.0f - MathUtil::Exp(-delta / component.footIKSmoothing), 0.0f, 1.0f);
    }

    component.leftFootOffset = MathUtil::Lerp(component.leftFootOffset, leftTarget, alpha);
    component.rightFootOffset = MathUtil::Lerp(component.rightFootOffset, rightTarget, alpha);
}

void WriteFootOffsets(const CharacterModelComponent& component, const Entity& meshEntity, AnimationGraphInstance& instance)
{
    // parts with partial rigs (clothing, hair) follow the body's pose but can't take foot IK
    const MeshComponent* meshComponent = meshEntity.TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->skeleton.IsValid() || !HumanoidSkeletonData::IsHumanoid(*meshComponent->skeleton))
    {
        return;
    }

    // the graph works in the skeleton's model space, which the mesh entity scales into the world
    const float worldScale = MathUtil::Max(MathUtil::Abs(meshEntity.GetWorldScale().y), 1e-6f);

    instance.SetFloat(NAME("LeftFootOffset"), component.leftFootOffset / worldScale);
    instance.SetFloat(NAME("RightFootOffset"), component.rightFootOffset / worldScale);
}

void ApplyRootMotion(const Entity& meshEntity, const AnimationGraphInstance& instance, CharacterControllerComponent& characterController, float delta)
{
    const float weight = instance.GetRootMotionWeight();

    if (weight <= 0.0f || delta <= 0.0f)
    {
        characterController.rootMotionVelocity = Vec3f::Zero();
        characterController.rootMotionWeight = 0.0f;

        return;
    }

    const Vec3f worldMotion = meshEntity.GetWorldRotation().RotateVector(instance.GetRootMotion() * meshEntity.GetWorldScale());

    characterController.rootMotionVelocity = worldMotion / delta;
    characterController.rootMotionWeight = weight;
}

} // namespace

Handle<AnimationGraph> GetDefaultCharacterGraph()
{
    const Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();

    if (!engineRegistry.IsValid())
    {
        return Handle<AnimationGraph>::empty;
    }

    return engineRegistry->GetAsset<AnimationGraph>(AssetBuckets::Animations, NAME("HumanoidLocomotion"));
}

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
            Entity* characterEntity = FindParentCharacterEntity(*entity);

            CharacterControllerComponent* characterController = characterEntity != nullptr
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
                component.hasBodyYaw = false;
                component.isTurnStepping = false;
                component.turnStepWeight = 0.0f;
                component.leftFootOffset = 0.0f;
                component.rightFootOffset = 0.0f;

                if (characterController != nullptr)
                {
                    characterController->rootMotionWeight = 0.0f;
                }

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
            {
                if (characterController != nullptr && !isRemotePlayer && characterController->movement.orientToMovement
                    && characterController->heading.LengthSquared() > 0.0001f)
                {
                    UpdateFacing(*entity, component, characterController->heading, 1.0f);
                }
                else
                {
                    UpdateFacing(*entity, component, movementDirection, GetFacingTurnAlpha(component, delta));
                }

                break;
            }
            case CharacterFacingMode::ViewDirection:
            {
                // Remote players' cameras aren't driven by anyone on this machine, so follow their movement instead
                if (characterController != nullptr && !isRemotePlayer)
                {
                    UpdateFacing(*entity, component, GetCharacterViewDirection(*characterEntity, *characterController), GetFacingTurnAlpha(component, delta));
                }
                else
                {
                    UpdateFacing(*entity, component, movementDirection, GetFacingTurnAlpha(component, delta));
                }

                break;
            }
            default:
                break;
            }

            const Handle<AnimationGraph> graph = component.graph.IsValid() ? component.graph : GetDefaultCharacterGraph();
            const TurnStepClips turnStepClips = graph.IsValid() ? FindTurnStepClips(*graph) : TurnStepClips {};

            // With no facing mode the model keeps whatever rotation it has
            if (component.hasFacingYaw)
            {
                UpdateBodyYaw(component, turnStepClips, delta);
                ApplyBodyRotation(*entity, component);
            }

            const Vec3f bodyForward = Vec3f(std::sin(component.bodyYaw), 0.0f, std::cos(component.bodyYaw));
            const Vec3f bodyRight = Vec3f::UnitY().Cross(bodyForward);
            const Vec2f localVelocity = Vec2f(horizontalVelocity.Dot(bodyRight), horizontalVelocity.Dot(bodyForward));

            component.smoothedLocalVelocity += (localVelocity - component.smoothedLocalVelocity) * MathUtil::Clamp(speedAlpha, 0.0f, 1.0f);

            if (component.facingMode == CharacterFacingMode::MovementDirection)
            {
                // The body turns to face its movement, so it always walks forward; strafing while it comes round reads as a sidestep
                component.moveAngle = 0.0f;
            }
            else if (component.smoothedLocalVelocity.Length() > IdleSpeedThreshold * 0.5f)
            {
                component.moveAngle = std::atan2(component.smoothedLocalVelocity.x, component.smoothedLocalVelocity.y);
            }

            if (!graph.IsValid())
            {
                continue;
            }

            CharacterMotionSignals signals = GatherSignals(component, turnStepClips);

            const Vec3f desiredVelocity = characterController != nullptr && !isRemotePlayer ? characterController->wishVelocity : horizontalVelocity;

            signals.velocity = component.smoothedLocalVelocity;
            signals.desiredVelocity = Vec2f(desiredVelocity.Dot(bodyRight), desiredVelocity.Dot(bodyForward));
            signals.trajectoryAcceleration = characterController != nullptr ? characterController->movement.groundAcceleration : 0.0f;
            signals.trajectoryTurnSharpness = component.turnSharpness;

            if (component.facingMode == CharacterFacingMode::ViewDirection)
            {
                signals.desiredFacing = MathUtil::RadToDeg(GetStandingTwist(component));
            }
            else if (component.facingMode == CharacterFacingMode::MovementDirection && signals.desiredVelocity.Length() > IdleSpeedThreshold)
            {
                signals.desiredFacing = MathUtil::RadToDeg(std::atan2(signals.desiredVelocity.x, signals.desiredVelocity.y));
            }

            const bool appliesRootMotion = component.applyRootMotion && characterController != nullptr && !isRemotePlayer;
            bool hasDrivenCharacter = false;

            float floorHeight = worldTranslation.y;

            if (characterController != nullptr)
            {
                floorHeight = characterEntity->GetWorldTranslation().y + GetCapsuleFeetOffset(*characterController, isPlaying);
            }

            Array<Entity*> bodyEntities;
            CollectBodyAnimatedEntities(*entity, bodyEntities);

            for (Entity* descendantEntity : bodyEntities)
            {
                AnimationComponent* animationComponent = &descendantEntity->GetComponent<AnimationComponent>();

                if (animationComponent->graph != graph)
                {
                    animationComponent->graph = graph;
                }

                // the animation system makes the instance on the graph's first update, so the very first frame has none
                if (!animationComponent->graphInstance.IsValid())
                {
                    continue;
                }

                WriteSignals(signals, *animationComponent->graphInstance);

                // the first animated mesh stands for the character's feet and root
                if (!hasDrivenCharacter)
                {
                    hasDrivenCharacter = true;

                    UpdateFootOffsets(component, *descendantEntity, GetWorld()->GetPhysicsWorld().Get(), floorHeight, delta);

                    if (appliesRootMotion)
                    {
                        ApplyRootMotion(*descendantEntity, *animationComponent->graphInstance, *characterController, delta);
                    }
                }

                WriteFootOffsets(component, *descendantEntity, *animationComponent->graphInstance);
            }

            if (characterController != nullptr && (!hasDrivenCharacter || !appliesRootMotion))
            {
                characterController->rootMotionWeight = 0.0f;
            }
        }
    }
}

} // namespace Hyperion
