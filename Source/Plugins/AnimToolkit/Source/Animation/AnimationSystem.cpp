/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>

#include <Animation/AnimationSystem.hpp>

#include <Scene/Components/MeshComponent.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Animation/AnimationPose.hpp>
#include <Animation/BoneMask.hpp>
#include <Animation/AnimationGraphInstance.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/MathUtil.hpp>

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
        return MathUtil::Mod(time, length);
    }

    if (time < 0.0f)
    {
        return length + MathUtil::Mod(time, length);
    }

    return time;
}

Animation* ResolveAnimation(
    const AnimationComponent& animationComponent,
    const MeshComponent& meshComponent)
{
    return animationComponent.animation.IsValid()
        ? animationComponent.animation.Get()
        : meshComponent.skeleton->GetAnimation(animationComponent.playbackState.animationIndex).Get();
}

const Animation* ResolveLayerAnimation(
    const MeshComponent& meshComponent,
    uint32 animationIndex,
    float weight)
{
    return weight > 0.0f && animationIndex != ~0u
        ? meshComponent.skeleton->GetAnimation(animationIndex).Get()
        : nullptr;
}

AnimationTrackLocks LockTracks(const Animation& animation)
{
    AnimationTrackLocks locks;

    for (const Handle<AnimationTrack>& track : animation.GetTracks())
    {
        Assert(track.IsValid(), "Animation '{}' has a null track", animation.GetName());

        locks.EmplaceBack(MakeUniqueWithAllocator<TSharedResLock<AssetObject>, AnimAllocator>(*track));
    }

    return locks;
}

bool IsStateFor(const AnimationInstanceState& state, const MeshComponent& meshComponent)
{
    return state.source.GetUnsafe() == meshComponent.skeleton.Get()
        && state.sourceRevision == meshComponent.skeleton->GetRevision();
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

    MeshComponent* meshComponent = entity->GetEntityManager()
        ? entity->TryGetComponent<MeshComponent>()
        : nullptr;

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
    state.instance->SetPose({});
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
    state.source = meshComponent.skeleton;
    state.sourceRevision = meshComponent.skeleton->GetRevision();
    state.instance = AcquireSkeletonInstance(*entity, meshComponent);
    state.pose = Array<Transform, AnimAllocator>(PoseOps::GetRestPose(meshComponent.skeleton->GetBindData()));

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

    if (meshComponent)
    {
        ReleaseSkeletonInstance(*entity, *meshComponent);
    }
}

// clips played from scripts or the inspector aren't always among the skeleton's own, which OnEntityAdded locked,
// so each bound clip holds its own tracks; binding through the skeleton retargets clips recorded on another humanoid rig
const ClipBinding& AnimationSystem::GetClipBinding(AnimationInstanceState& state, const Animation& animation, const Skeleton& skeleton)
{
    const WeakHandle<Animation> key = animation.WeakHandleFromThis();

    auto it = state.clipBindings.Find(key);

    if (it == state.clipBindings.End())
    {
        for (auto expiredIt = state.clipBindings.Begin(); expiredIt != state.clipBindings.End();)
        {
            if (expiredIt->first.Expired())
            {
                expiredIt = state.clipBindings.Erase(expiredIt);

                continue;
            }

            ++expiredIt;
        }

        it = state.clipBindings.Insert(key, AnimationBoundClip { PoseOps::BindClip(animation, skeleton), LockTracks(animation) }).first;
    }
    else if (!PoseOps::IsBindingCurrent(it->second.binding, animation))
    {
        it->second.binding = PoseOps::BindClip(animation, skeleton);
        it->second.trackLocks = LockTracks(animation);
    }

    return it->second.binding;
}

void AnimationSystem::EvaluatePose(
    Entity* entity,
    const AnimationComponent& animationComponent,
    MeshComponent& meshComponent,
    const Animation& animation,
    float time,
    float blend)
{
    const AnimationPlaybackState& playbackState = animationComponent.playbackState;

    auto stateIt = m_instanceStates.Find(entity);

    AnimationInstanceState& state = stateIt != m_instanceStates.End() && IsStateFor(stateIt->second, meshComponent)
        ? stateIt->second
        : CreateInstanceState(entity, meshComponent);

    const Skeleton& skeleton = *meshComponent.skeleton;
    const SkeletonBindData& bindData = skeleton.GetBindData();

    if (state.pose.Size() != bindData.NumSlots())
    {
        state.pose = Array<Transform, AnimAllocator>(PoseOps::GetRestPose(bindData));
    }

    const Animation* layerAnimation = ResolveLayerAnimation(
        meshComponent,
        playbackState.layerAnimationIndex,
        playbackState.layerWeight);

    const Animation* secondLayerAnimation = ResolveLayerAnimation(
        meshComponent,
        playbackState.secondLayerAnimationIndex,
        playbackState.secondLayerWeight);

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
                const BoneMask& layerMask = *animationComponent.layerMask;

                if (state.layerMask.GetUnsafe() != &layerMask || state.layerMaskRevision != layerMask.GetRevision() || state.layerMaskWeights.Size() != bindData.NumSlots())
                {
                    state.layerMask = layerMask.WeakHandleFromThis();
                    state.layerMaskRevision = layerMask.GetRevision();
                    state.layerMaskWeights = layerMask.BuildSlotWeights(bindData);
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
        GetClipBinding(state, animation, skeleton);
        GetClipBinding(state, *params.layerAnimation, skeleton);

        if (params.secondLayerAnimation != nullptr)
        {
            GetClipBinding(state, *params.secondLayerAnimation, skeleton);
        }

        PoseOps::ApplyLayered(
            animation,
            GetClipBinding(state, animation, skeleton),
            &GetClipBinding(state, *params.layerAnimation, skeleton),
            params.secondLayerAnimation != nullptr ? &GetClipBinding(state, *params.secondLayerAnimation, skeleton) : nullptr,
            bindData,
            params,
            state.pose.ToSpan());
    }
    else
    {
        PoseOps::ApplyBlended(animation, GetClipBinding(state, animation, skeleton), bindData, params, state.pose.ToSpan());
    }

    state.renderPose = state.pose;

    if (playbackState.twistAngle != 0.0f
        && playbackState.twistRootBone.IsValid()
        && playbackState.twistEndBone.IsValid())
    {
        PoseOps::ApplyTwist(bindData, playbackState.twistRootBone, playbackState.twistEndBone, playbackState.twistAngle, state.renderPose.ToSpan());
    }

    state.instance->SetPose(state.renderPose.ToSpan());
}

void AnimationSystem::EvaluateGraph(Entity* entity, AnimationComponent& animationComponent, MeshComponent& meshComponent, float delta)
{
    auto stateIt = m_instanceStates.Find(entity);

    AnimationInstanceState& state = stateIt != m_instanceStates.End() && IsStateFor(stateIt->second, meshComponent)
        ? stateIt->second
        : CreateInstanceState(entity, meshComponent);

    const Handle<AnimationGraphInstance> previousInstance = animationComponent.graphInstance;

    if (!previousInstance.IsValid()
        || previousInstance->GetGraph() != animationComponent.graph
        || previousInstance->GetGraphRevision() != animationComponent.graph->GetRevision())
    {
        animationComponent.graphInstance = MakeHandle<AnimationGraphInstance>(animationComponent.graph);
        InitObject(animationComponent.graphInstance);

        // an edited graph keeps its parameter values, and the graph document stays attached
        if (previousInstance.IsValid())
        {
            animationComponent.graphInstance->CopyParameterValues(*previousInstance);
            animationComponent.graphInstance->SetDebugCapture(previousInstance->IsDebugCaptured());
        }
    }

    // the graph's clips aren't always among the skeleton's own, which OnEntityAdded locked; slots lock their own clips
    if (state.lockedGraph.GetUnsafe() != animationComponent.graph.Get() || state.lockedGraphRevision != animationComponent.graph->GetRevision())
    {
        AnimationTrackLocks graphTrackLocks;

        for (const Handle<Animation>& clip : animationComponent.graph->CollectClips())
        {
            for (auto& lock : LockTracks(*clip))
            {
                graphTrackLocks.PushBack(std::move(lock));
            }
        }

        state.graphTrackLocks = std::move(graphTrackLocks);
        state.lockedGraph = WeakHandle<AnimationGraph>(animationComponent.graph);
        state.lockedGraphRevision = animationComponent.graph->GetRevision();
    }

    const Skeleton& skeleton = *meshComponent.skeleton;
    const SkeletonBindData& bindData = skeleton.GetBindData();

    state.pose.Resize(bindData.NumSlots());

    animationComponent.graphInstance->Update(delta, skeleton);
    animationComponent.graphInstance->Evaluate(skeleton, state.pose.ToSpan());

    state.renderPose = state.pose;

    state.instance->SetPose(state.renderPose.ToSpan());
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
