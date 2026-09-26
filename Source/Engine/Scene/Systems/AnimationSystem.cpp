/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Systems/AnimationSystem.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Framework/GameState.hpp>

#include <cmath>

#include <AnimationSystem.generated.inl>

namespace Hyperion {

namespace /* Helpers */ {

void ApplyTwist(Skeleton& skeleton, Name rootBoneName, Name endBoneName, float angle)
{
    Bone* endBone = skeleton.FindBone(endBoneName);

    if (!endBone)
    {
        return;
    }

    Array<Bone*> chain;

    for (Node* node = endBone; node != nullptr; node = node->GetParent())
    {
        Bone* bone = DynamicCast<Bone>(node);

        if (!bone)
        {
            return;
        }

        chain.PushBack(bone);

        if (bone->GetBoneName() == rootBoneName)
        {
            break;
        }
    }

    if (chain.Empty() || chain.Back()->GetBoneName() != rootBoneName)
    {
        return;
    }

    // an even share per bone, root first; each bone's children follow it, so every bone only adds its own share
    const Quat4f shareRotation(Vec3f::UnitY(), -angle / float(chain.Size()));

    for (size_t index = chain.Size(); index > 0; --index)
    {
        Bone* bone = chain[index - 1];

        bone->SetWorldRotation(shareRotation * bone->GetWorldRotation());
    }
}

float AdvanceLayerTime(float time, float step, float length)
{
    time += step;

    if (length <= 0.0f)
    {
        return 0.0f;
    }

    if (time > length)
    {
        return std::fmod(time, length);
    }

    if (time < 0.0f)
    {
        return length + std::fmod(time, length);
    }

    return time;
}
} // namespace

bool AnimationSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void AnimationSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    const MeshComponent& meshComponent = entity->GetEntityManager()->GetComponent<MeshComponent>(entity);
    InitObject(meshComponent.skeleton);

    if (meshComponent.skeleton.IsValid())
    {
        auto& locks = m_resourceHandles[entity];
        locks.Clear();

        for (const Handle<Animation>& anim : meshComponent.skeleton->GetAnimations())
        {
            for (const Handle<AnimationTrack>& track : anim->GetTracks())
            {
                locks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
            }
        }
    }

    AnimationComponent& animationComponent = entity->GetEntityManager()->GetComponent<AnimationComponent>(entity);
    
    // reset the animation state to default values
    animationComponent.playbackState = {};
    animationComponent.playbackState.animationIndex = 0;
    animationComponent.playbackState.status = AnimationPlaybackStatus::PLAYING;
    animationComponent.playbackState.loopMode = AnimationLoopMode::REPEAT;
    animationComponent.playbackState.speed = 1.0f;
    animationComponent.playbackState.currentTime = 0.0f;
}

void AnimationSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    auto it = m_resourceHandles.Find(entity);

    if (it != m_resourceHandles.End())
    {
        m_resourceHandles.Erase(it);
    }
}

void AnimationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!GetWorld()->GetGameState().IsSimulating())
    {
        return;
    }

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, animationComponent, meshComponent] : scene->GetEntityManager()->GetEntitySet<AnimationComponent, MeshComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!meshComponent.skeleton)
            {
                continue;
            }

            AnimationPlaybackState& playbackState = animationComponent.playbackState;

            if (playbackState.status == AnimationPlaybackStatus::PLAYING)
            {
                if (playbackState.animationIndex == ~0u)
                {
                    playbackState = {};

                    continue;
                }

                Animation* animation = meshComponent.skeleton->GetAnimation(playbackState.animationIndex);
                if (!animation)
                {
                    HYP_LOG(Animation, Warning, "AnimationComponent has a playing animation but the associated Skeleton asset has no such animation (index {})", playbackState.animationIndex);

                    playbackState = {};

                    continue;
                }

                playbackState.currentTime += delta * playbackState.speed;

                const float animationLength = animation->GetLength();

                if (playbackState.currentTime > animationLength)
                {
                    if (playbackState.loopMode == AnimationLoopMode::ONCE)
                    {
                        // hold the last frame instead of snapping back to frame 0
                        playbackState.status = AnimationPlaybackStatus::STOPPED;
                        playbackState.currentTime = animationLength;
                    }
                    else
                    {
                        playbackState.currentTime = animationLength > 0.0f
                            ? std::fmod(playbackState.currentTime, animationLength)
                            : 0.0f;
                    }
                }
                else if (playbackState.currentTime < 0.0f)
                {
                    // playing in reverse
                    if (playbackState.loopMode == AnimationLoopMode::ONCE)
                    {
                        playbackState.status = AnimationPlaybackStatus::STOPPED;
                        playbackState.currentTime = 0.0f;
                    }
                    else
                    {
                        playbackState.currentTime = animationLength > 0.0f
                            ? animationLength + std::fmod(playbackState.currentTime, animationLength)
                            : 0.0f;
                    }
                }

                const Animation* layerAnimation = playbackState.layerWeight > 0.0f && playbackState.layerAnimationIndex != ~0u
                    ? meshComponent.skeleton->GetAnimation(playbackState.layerAnimationIndex).Get()
                    : nullptr;

                const Animation* secondLayerAnimation = playbackState.secondLayerWeight > 0.0f && playbackState.secondLayerAnimationIndex != ~0u
                    ? meshComponent.skeleton->GetAnimation(playbackState.secondLayerAnimationIndex).Get()
                    : nullptr;

                if (layerAnimation != nullptr)
                {
                    playbackState.layerTime = AdvanceLayerTime(playbackState.layerTime, delta * playbackState.speed, layerAnimation->GetLength());
                }

                if (secondLayerAnimation != nullptr)
                {
                    playbackState.secondLayerTime = AdvanceLayerTime(playbackState.secondLayerTime, delta * playbackState.speed, secondLayerAnimation->GetLength());
                }

                const Animation* overlayAnimation = playbackState.overlayWeight > 0.0f && playbackState.overlayAnimationIndex != ~0u
                    ? meshComponent.skeleton->GetAnimation(playbackState.overlayAnimationIndex).Get()
                    : nullptr;

                if (overlayAnimation != nullptr)
                {
                    playbackState.overlayTime = AdvanceLayerTime(playbackState.overlayTime, delta * playbackState.speed, overlayAnimation->GetLength());
                }

                if (layerAnimation != nullptr || secondLayerAnimation != nullptr || overlayAnimation != nullptr)
                {
                    ApplyAnimParams params {};
                    params.time = playbackState.currentTime;
                    params.blend = 0.5f;

                    if (layerAnimation != nullptr)
                    {
                        params.layerAnimation = layerAnimation;
                        params.layerTime = playbackState.layerTime;
                        params.layerWeight = playbackState.layerWeight;
                        params.layerExcludedBone = playbackState.layerExcludedBone;

                        params.secondLayerAnimation = secondLayerAnimation;
                        params.secondLayerTime = playbackState.secondLayerTime;
                        params.secondLayerWeight = playbackState.secondLayerWeight;
                    }
                    else if (secondLayerAnimation != nullptr)
                    {
                        params.layerAnimation = secondLayerAnimation;
                        params.layerTime = playbackState.secondLayerTime;
                        params.layerWeight = playbackState.secondLayerWeight;
                    }

                    params.overlayAnimation = overlayAnimation;
                    params.overlayTime = playbackState.overlayTime;
                    params.overlayWeight = playbackState.overlayWeight;
                    params.overlayRootBone = playbackState.overlayRootBone;
                    params.overlaySecondRootBone = playbackState.overlaySecondRootBone;

                    animation->ApplyLayered(*meshComponent.skeleton, params);
                }
                else
                {
                    animation->ApplyBlended(meshComponent.skeleton, playbackState.currentTime, 0.5f);
                }

                if (playbackState.twistAngle != 0.0f && playbackState.twistRootBone.IsValid() && playbackState.twistEndBone.IsValid())
                {
                    ApplyTwist(*meshComponent.skeleton, playbackState.twistRootBone, playbackState.twistEndBone, playbackState.twistAngle);
                }

                meshComponent.skeleton->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

} // namespace Hyperion
