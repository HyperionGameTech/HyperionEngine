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

static constexpr float SpeedSmoothingSharpness = 10.0f;
static constexpr float MaxTrackedSpeed = 50.0f;
static constexpr float IdleSpeedThreshold = 0.3f;
static constexpr float MinPlaybackSpeed = 0.6f;
static constexpr float MaxPlaybackSpeed = 1.6f;

// How much of the movement has to point away from the facing direction to count as backpedalling
static constexpr float BackwardSpeedFraction = 0.5f;

struct LocomotionAnimation
{
    uint32 animationIndex = ~0u;
    float playbackSpeed = 1.0f;
    bool holdFirstFrame = false;
};

bool CharacterModelSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | SceneFlags::EDITOR | ExpectedFlags)) == ExpectedFlags;
}

static Entity* FindParentCharacterEntity(const Entity& entity)
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

static Vec3f GetCharacterViewDirection(const Entity& characterEntity, const CharacterControllerComponent& characterController)
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

static float GetCapsuleFeetOffset(const CharacterControllerComponent& characterController, bool isPlaying)
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

static uint32 FindAnimationIndex(const Skeleton& skeleton, Name animationName)
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

static LocomotionAnimation SelectLocomotionAnimation(const CharacterModelComponent& component, const Skeleton& skeleton, bool isMovingBackward)
{
    const uint32 idleIndex = FindAnimationIndex(skeleton, component.idleAnimation);
    uint32 walkIndex = FindAnimationIndex(skeleton, component.walkAnimation);
    uint32 runIndex = FindAnimationIndex(skeleton, component.runAnimation);

    if (walkIndex == ~0u)
    {
        walkIndex = runIndex;
    }

    if (runIndex == ~0u)
    {
        runIndex = walkIndex;
    }

    const float speed = component.smoothedSpeed;

    LocomotionAnimation result;

    if (speed < IdleSpeedThreshold)
    {
        if (idleIndex != ~0u)
        {
            result.animationIndex = idleIndex;
        }
        else
        {
            result.animationIndex = walkIndex;
            result.holdFirstFrame = true;
        }

        return result;
    }

    const float walkReferenceSpeed = MathUtil::Max(component.walkReferenceSpeed, 0.01f);
    const float runReferenceSpeed = MathUtil::Max(component.runReferenceSpeed, walkReferenceSpeed);
    const float runThreshold = (walkReferenceSpeed + runReferenceSpeed) * 0.5f;

    const bool isRunning = speed >= runThreshold;

    result.animationIndex = isRunning ? runIndex : walkIndex;
    result.playbackSpeed = MathUtil::Clamp(speed / (isRunning ? runReferenceSpeed : walkReferenceSpeed), MinPlaybackSpeed, MaxPlaybackSpeed);

    // No backpedal clip to pick, so run the forward cycle in reverse
    if (isMovingBackward)
    {
        result.playbackSpeed = -result.playbackSpeed;
    }

    return result;
}

static void ApplyLocomotionAnimation(AnimationComponent& animationComponent, const Skeleton& skeleton, const LocomotionAnimation& locomotion)
{
    AnimationPlaybackState& playbackState = animationComponent.playbackState;

    if (locomotion.animationIndex == ~0u)
    {
        return;
    }

    if (playbackState.animationIndex != locomotion.animationIndex)
    {
        // carry the cycle phase across so walk <-> run transitions keep the feet in step
        const Animation* previousAnimation = skeleton.GetAnimation(playbackState.animationIndex).Get();
        const Animation* nextAnimation = skeleton.GetAnimation(locomotion.animationIndex).Get();

        float phase = 0.0f;

        if (previousAnimation != nullptr && previousAnimation->GetLength() > 0.0f)
        {
            phase = playbackState.currentTime / previousAnimation->GetLength();
        }

        playbackState.animationIndex = locomotion.animationIndex;
        playbackState.currentTime = nextAnimation != nullptr ? phase * nextAnimation->GetLength() : 0.0f;
    }

    playbackState.status = AnimationPlaybackStatus::PLAYING;
    playbackState.loopMode = AnimationLoopMode::REPEAT;

    if (locomotion.holdFirstFrame)
    {
        playbackState.currentTime = 0.0f;
        playbackState.speed = 0.0f;
    }
    else
    {
        playbackState.speed = locomotion.playbackSpeed;
    }
}

static void UpdateFacing(Entity& entity, CharacterModelComponent& component, const Vec3f& facingDirection, float delta)
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

    const float modelYaw = component.facingYaw + MathUtil::DegToRad(component.forwardYawOffset);

    // Transforms store the inverse rotation, so negate the yaw to turn +Z toward the movement direction
    entity.SetWorldRotation(Quat4f(Vec3f::UnitY(), -modelYaw), TransformChangeType::Simulation);
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

                continue;
            }

            const Vec3f worldTranslation = entity->GetWorldTranslation();

            Vec3f horizontalVelocity = Vec3f::Zero();

            if (component.hasPreviousTranslation && delta > 0.0f)
            {
                const Vec3f displacement = worldTranslation - component.previousTranslation;

                horizontalVelocity = Vec3f(displacement.x, 0.0f, displacement.z) * (1.0f / delta);

                if (horizontalVelocity.Length() > MaxTrackedSpeed)
                {
                    // teleport or reconciliation snap
                    horizontalVelocity = Vec3f::Zero();
                }
            }

            component.previousTranslation = worldTranslation;
            component.hasPreviousTranslation = true;

            const float speedAlpha = 1.0f - MathUtil::Exp(-SpeedSmoothingSharpness * delta);
            component.smoothedSpeed = MathUtil::Lerp(component.smoothedSpeed, horizontalVelocity.Length(), MathUtil::Clamp(speedAlpha, 0.0f, 1.0f));

            const Vec3f movementDirection = horizontalVelocity.Length() >= IdleSpeedThreshold
                ? horizontalVelocity
                : Vec3f::Zero();

            switch (component.facingMode)
            {
            case CharacterFacingMode::MovementDirection:
                UpdateFacing(*entity, component, movementDirection, delta);

                break;
            case CharacterFacingMode::ViewDirection:
            {
                // Remote players' cameras aren't driven by anyone on this machine, so follow their movement instead
                const PlayerComponent* playerComponent = characterEntity != nullptr ? characterEntity->TryGetComponent<PlayerComponent>() : nullptr;
                const bool isRemotePlayer = playerComponent != nullptr && !playerComponent->IsLocalPlayer();

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

                const Skeleton& skeleton = *meshComponent->skeleton.Get();

                ApplyLocomotionAnimation(*animationComponent, skeleton, SelectLocomotionAnimation(component, skeleton, isMovingBackward));
            }
        }
    }
}

} // namespace Hyperion
