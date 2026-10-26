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
#include <Scene/Animation/AnimationPose.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/BoneMask.hpp>
#include <Scene/Animation/AnimationGraphInstance.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Framework/GameState.hpp>

#include <cmath>

#include <AnimationSystem.generated.inl>

namespace Hyperion {

namespace /* Helpers */ {

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

Animation* ResolveAnimation(const AnimationComponent& animationComponent, const MeshComponent& meshComponent)
{
    return animationComponent.animation.IsValid()
        ? animationComponent.animation.Get()
        : meshComponent.skeleton->GetAnimation(animationComponent.playbackState.animationIndex).Get();
}

const Animation* ResolveLayerAnimation(const MeshComponent& meshComponent, uint32 animationIndex, float weight)
{
    return weight > 0.0f && animationIndex != ~0u
        ? meshComponent.skeleton->GetAnimation(animationIndex).Get()
        : nullptr;
}
} // namespace

AnimationSystem::~AnimationSystem() = default;

bool AnimationSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void AnimationSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = entity->GetEntityManager()->GetComponent<MeshComponent>(entity);
    InitObject(meshComponent.skeleton);

    AnimationComponent& animationComponent = entity->GetEntityManager()->GetComponent<AnimationComponent>(entity);

    if (meshComponent.skeleton.IsValid())
    {
        auto& locks = m_resourceHandles[entity];
        locks.Clear();

        const auto lockTracks = [&locks](const Animation& animation)
        {
            for (const Handle<AnimationTrack>& track : animation.GetTracks())
            {
                Assert(track.IsValid());

                if (!track.IsValid())
                {
                    continue;
                }

                locks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
            }
        };

        for (const Handle<Animation>& anim : meshComponent.skeleton->GetAnimations())
        {
            Assert(anim.IsValid());

            if (!anim.IsValid())
            {
                continue;
            }

            lockTracks(*anim);
        }

        CreateInstanceState(entity, meshComponent);
    }

    AnimationPlaybackState& playbackState = animationComponent.playbackState;

    if (!animationComponent.animation.IsValid() && playbackState.animationIndex == ~0u)
    {
        playbackState.animationIndex = 0;
        playbackState.status = AnimationPlaybackStatus::PLAYING;
        playbackState.loopMode = AnimationLoopMode::REPEAT;
    }
}

void AnimationSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    auto it = m_resourceHandles.Find(entity);

    if (it != m_resourceHandles.End())
    {
        m_resourceHandles.Erase(it);
    }

    MeshComponent* meshComponent = entity->GetEntityManager() ? entity->TryGetComponent<MeshComponent>() : nullptr;

    DestroyInstanceState(entity, meshComponent);
}

void AnimationSystem::SetPreviewTime(Entity* entity, float time)
{
    auto it = m_instanceStates.Find(entity);

    if (it == m_instanceStates.End())
    {
        return;
    }

    if (!it->second.isPreviewing)
    {
        it->second.isPreviewing = true;
        ++m_numPreviewing;
    }

    it->second.previewTime = time;
}

void AnimationSystem::AdvanceGraphPreview(Entity* entity, float delta)
{
    auto it = m_instanceStates.Find(entity);

    if (it == m_instanceStates.End())
    {
        return;
    }

    if (!it->second.isPreviewing)
    {
        it->second.isPreviewing = true;
        ++m_numPreviewing;
    }

    it->second.previewDelta += delta;
}

void AnimationSystem::StopPreview(Entity* entity)
{
    auto it = m_instanceStates.Find(entity);

    if (it == m_instanceStates.End() || !it->second.isPreviewing)
    {
        return;
    }

    AnimationInstanceState& state = it->second;
    state.isPreviewing = false;
    --m_numPreviewing;

    state.pose.Clear();
    state.renderPose.Clear();
    state.instance->SetPoseMatrices({}, {});
}

Span<const Transform> AnimationSystem::GetRenderPose(Entity* entity) const
{
    auto it = m_instanceStates.Find(entity);

    if (it == m_instanceStates.End())
    {
        return {};
    }

    return it->second.renderPose.ToSpan();
}

AnimationInstanceState& AnimationSystem::CreateInstanceState(Entity* entity, MeshComponent& meshComponent)
{
    DestroyInstanceState(entity, &meshComponent);

    AnimationInstanceState& state = m_instanceStates[entity];
    state.source = meshComponent.skeleton.Get();
    state.sourceRevision = meshComponent.skeleton->GetRevision();
    state.instance = meshComponent.skeleton->CreateInstance();
    state.pose = PoseOps::MakeRestPose(meshComponent.skeleton->GetBindData());

    meshComponent.skeletonInstance = state.instance;

    entity->SetNeedsRenderProxyUpdate();

    return state;
}

void AnimationSystem::DestroyInstanceState(Entity* entity, MeshComponent* meshComponent)
{
    auto it = m_instanceStates.Find(entity);

    if (it != m_instanceStates.End())
    {
        if (it->second.isPreviewing)
        {
            --m_numPreviewing;
        }

        m_instanceStates.Erase(it);
    }

    // the render proxies may still point at the instance for a few frames
    if (meshComponent && meshComponent->skeletonInstance.IsValid())
    {
        EnqueueDeletion(std::move(meshComponent->skeletonInstance));

        entity->SetNeedsRenderProxyUpdate();
    }
}

const ClipBinding& AnimationSystem::GetClipBinding(Entity* entity, AnimationInstanceState& state, const Animation& animation, const SkeletonBindData& bindData)
{
    auto it = state.clipBindings.Find(&animation);

    if (it != state.clipBindings.End() && it->second.animationRevision == animation.GetRevision())
    {
        return it->second;
    }

    // clips played from scripts or the inspector aren't always among the skeleton's own, which OnEntityAdded locked
    auto& locks = m_resourceHandles[entity];

    for (const Handle<AnimationTrack>& track : animation.GetTracks())
    {
        if (track.IsValid())
        {
            locks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
        }
    }

    if (it != state.clipBindings.End())
    {
        it->second = PoseOps::BindClip(animation, bindData);

        return it->second;
    }

    return state.clipBindings.Insert({ &animation, PoseOps::BindClip(animation, bindData) }).first->second;
}

void AnimationSystem::EvaluatePose(Entity* entity, const AnimationComponent& animationComponent, MeshComponent& meshComponent, const Animation& animation, float time, float blend)
{
    const AnimationPlaybackState& playbackState = animationComponent.playbackState;

    auto stateIt = m_instanceStates.Find(entity);

    AnimationInstanceState& state = stateIt != m_instanceStates.End() && stateIt->second.source == meshComponent.skeleton.Get()
        && stateIt->second.sourceRevision == meshComponent.skeleton->GetRevision()
        ? stateIt->second
        : CreateInstanceState(entity, meshComponent);

    const SkeletonBindData& bindData = meshComponent.skeleton->GetBindData();

    if (state.pose.Size() != bindData.NumSlots())
    {
        state.pose = PoseOps::MakeRestPose(bindData);
    }

    const Animation* layerAnimation = ResolveLayerAnimation(meshComponent, playbackState.layerAnimationIndex, playbackState.layerWeight);
    const Animation* secondLayerAnimation = ResolveLayerAnimation(meshComponent, playbackState.secondLayerAnimationIndex, playbackState.secondLayerWeight);

    ApplyAnimParams params {};
    params.time = time;
    params.blend = blend;

    if (layerAnimation != nullptr || secondLayerAnimation != nullptr)
    {
        if (layerAnimation != nullptr)
        {
            params.layerAnimation = layerAnimation;
            params.layerTime = playbackState.layerTime;
            params.layerWeight = playbackState.layerWeight;
            params.layerExcludedBone = playbackState.layerExcludedBone;

            if (animationComponent.layerMask.IsValid())
            {
                if (state.layerMask != animationComponent.layerMask.Get() || state.layerMaskWeights.Size() != bindData.NumSlots())
                {
                    state.layerMask = animationComponent.layerMask.Get();
                    state.layerMaskWeights = animationComponent.layerMask->BuildSlotWeights(bindData);
                }

                params.layerBoneWeights = state.layerMaskWeights.ToSpan();
            }

            params.secondLayerAnimation = secondLayerAnimation;
            params.secondLayerTime = playbackState.secondLayerTime;
            params.secondLayerWeight = playbackState.secondLayerWeight;
        }
        else
        {
            params.layerAnimation = secondLayerAnimation;
            params.layerTime = playbackState.secondLayerTime;
            params.layerWeight = playbackState.secondLayerWeight;
        }

        // bind everything first; a later insert may move the bindings already fetched
        GetClipBinding(entity, state, animation, bindData);
        GetClipBinding(entity, state, *params.layerAnimation, bindData);

        if (params.secondLayerAnimation != nullptr)
        {
            GetClipBinding(entity, state, *params.secondLayerAnimation, bindData);
        }

        PoseOps::ApplyLayered(
            animation,
            GetClipBinding(entity, state, animation, bindData),
            &GetClipBinding(entity, state, *params.layerAnimation, bindData),
            params.secondLayerAnimation != nullptr ? &GetClipBinding(entity, state, *params.secondLayerAnimation, bindData) : nullptr,
            bindData,
            params,
            state.pose.ToSpan());
    }
    else
    {
        PoseOps::ApplyBlended(animation, GetClipBinding(entity, state, animation, bindData), bindData, params, state.pose.ToSpan());
    }

    state.renderPose = state.pose;

    if (playbackState.twistAngle != 0.0f
        && playbackState.twistRootBone.IsValid()
        && playbackState.twistEndBone.IsValid())
    {
        PoseOps::ApplyTwist(bindData, playbackState.twistRootBone, playbackState.twistEndBone, playbackState.twistAngle, state.renderPose.ToSpan());
    }

    state.modelMatrices.Resize(bindData.NumSlots());
    state.skinningMatrices.Resize(bindData.NumSlots());

    PoseOps::ComputeMatrices(bindData, state.renderPose.ToSpan(), state.modelMatrices.ToSpan(), state.skinningMatrices.ToSpan());

    state.instance->SetPoseMatrices(state.modelMatrices.ToSpan(), state.skinningMatrices.ToSpan());
}

void AnimationSystem::EvaluateGraph(Entity* entity, AnimationComponent& animationComponent, MeshComponent& meshComponent, float delta)
{
    auto stateIt = m_instanceStates.Find(entity);

    AnimationInstanceState& state = stateIt != m_instanceStates.End()
            && stateIt->second.source == meshComponent.skeleton.Get()
            && stateIt->second.sourceRevision == meshComponent.skeleton->GetRevision()
        ? stateIt->second
        : CreateInstanceState(entity, meshComponent);

    const Handle<AnimationGraphInstance> previousInstance = animationComponent.graphInstance;

    if (!previousInstance.IsValid()
        || previousInstance->GetGraph() != animationComponent.graph
        || previousInstance->GetGraphRevision() != animationComponent.graph->GetRevision())
    {
        animationComponent.graphInstance = MakeHandle<AnimationGraphInstance>(animationComponent.graph);
        InitObject(animationComponent.graphInstance);

        // an edited graph keeps the values its parameters were already set to
        if (previousInstance.IsValid())
        {
            animationComponent.graphInstance->CopyParameterValues(*previousInstance);
        }

        // the graph's clips aren't always among the skeleton's own, which OnEntityAdded locked
        auto& locks = m_resourceHandles[entity];

        for (const Handle<Animation>& clip : animationComponent.graph->CollectClips())
        {
            for (const Handle<AnimationTrack>& track : clip->GetTracks())
            {
                if (track.IsValid())
                {
                    locks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
                }
            }
        }
    }

    const Skeleton& skeleton = *meshComponent.skeleton;
    const SkeletonBindData& bindData = skeleton.GetBindData();

    state.pose.Resize(bindData.NumSlots());

    animationComponent.graphInstance->Update(delta, skeleton);
    animationComponent.graphInstance->Evaluate(skeleton, state.pose.ToSpan());

    state.renderPose = state.pose;

    state.modelMatrices.Resize(bindData.NumSlots());
    state.skinningMatrices.Resize(bindData.NumSlots());

    PoseOps::ComputeMatrices(bindData, state.renderPose.ToSpan(), state.modelMatrices.ToSpan(), state.skinningMatrices.ToSpan());

    state.instance->SetPoseMatrices(state.modelMatrices.ToSpan(), state.skinningMatrices.ToSpan());
}

void AnimationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    const bool isSimulating = GetWorld()->GetGameState().IsSimulating();

    if (!isSimulating && m_numPreviewing == 0)
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

            if (!isSimulating)
            {
                auto stateIt = m_instanceStates.Find(entity);

                if (stateIt == m_instanceStates.End() || !stateIt->second.isPreviewing)
                {
                    continue;
                }

                if (animationComponent.graph.IsValid())
                {
                    EvaluateGraph(entity, animationComponent, meshComponent, stateIt->second.previewDelta);
                    stateIt->second.previewDelta = 0.0f;

                    continue;
                }

                // a preview snaps to the scrubbed time instead of easing toward it
                if (const Animation* animation = ResolveAnimation(animationComponent, meshComponent))
                {
                    EvaluatePose(entity, animationComponent, meshComponent, *animation, stateIt->second.previewTime, 1.0f);
                }

                continue;
            }

            if (animationComponent.graph.IsValid())
            {
                EvaluateGraph(entity, animationComponent, meshComponent, delta);

                continue;
            }

            AnimationPlaybackState& playbackState = animationComponent.playbackState;

            if (playbackState.status != AnimationPlaybackStatus::PLAYING)
            {
                continue;
            }

            if (!animationComponent.animation.IsValid() && playbackState.animationIndex == ~0u)
            {
                playbackState = {};

                continue;
            }

            Animation* animation = ResolveAnimation(animationComponent, meshComponent);

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

            if (const Animation* layerAnimation = ResolveLayerAnimation(meshComponent, playbackState.layerAnimationIndex, playbackState.layerWeight))
            {
                playbackState.layerTime = AdvanceLayerTime(playbackState.layerTime, delta * playbackState.speed, layerAnimation->GetLength());
            }

            if (const Animation* secondLayerAnimation = ResolveLayerAnimation(meshComponent, playbackState.secondLayerAnimationIndex, playbackState.secondLayerWeight))
            {
                playbackState.secondLayerTime = AdvanceLayerTime(playbackState.secondLayerTime, delta * playbackState.speed, secondLayerAnimation->GetLength());
            }

            const float blend = PoseOps::AdvanceCrossFade(delta, animationComponent.crossFadeRemaining, 0.5f);

            EvaluatePose(entity, animationComponent, meshComponent, *animation, playbackState.currentTime, blend);
        }
    }
}

} // namespace Hyperion
