/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/AnimationSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/animation/Animation.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/reflection/Handle.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <AnimationSystem.generated.inl>

namespace hyperion {

void AnimationSystem::OnEntityAdded(Entity* entity)
{
    HYP_SCOPE;

    SystemBase::OnEntityAdded(entity);

    const MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);
    InitObject(meshComponent.skeleton);

    if (meshComponent.skeleton.IsValid())
    {
        if (const Handle<SkeletonAsset>& skeletonAsset = meshComponent.skeleton->GetAsset())
        {
            m_resourceHandles.Set(meshComponent.skeleton.Get(), ResourceHandle(*skeletonAsset->GetResource()));
        }
    }
}

void AnimationSystem::OnEntityRemoved(Entity* entity)
{
    HYP_SCOPE;

    SystemBase::OnEntityRemoved(entity);

    const AnimationComponent& animationComponent = GetEntityManager().GetComponent<AnimationComponent>(entity);
    const MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (meshComponent.skeleton.IsValid())
    {
        m_resourceHandles.Erase(meshComponent.skeleton.Get());
    }
}

void AnimationSystem::Process(float delta)
{
    HYP_SCOPE;

    for (auto [entity, animationComponent, meshComponent] : GetEntityManager().GetEntitySet<AnimationComponent, MeshComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!meshComponent.skeleton)
        {
            continue;
        }

        AnimationPlaybackState& playbackState = animationComponent.playbackState;

        if (playbackState.status == AnimationPlaybackStatus::PLAYING)
        {
            const Handle<SkeletonAsset>& skeletonAsset = meshComponent.skeleton->GetAsset();
            Assert(skeletonAsset != nullptr);

            auto resourceHandleIt = m_resourceHandles.Find(meshComponent.skeleton.Get());
            if (resourceHandleIt == m_resourceHandles.End())
            {
                resourceHandleIt = m_resourceHandles.Insert(meshComponent.skeleton.Get(), ResourceHandle(*skeletonAsset->GetResource())).first;
            }

            SkeletonData* skeletonData = skeletonAsset->GetSkeletonData();
            Assert(skeletonData != nullptr);

            if (playbackState.animationIndex == ~0u)
            {
                playbackState = {};

                continue;
            }

            Animation* animation = skeletonData->GetAnimation(playbackState.animationIndex);
            if (!animation)
            {
                HYP_LOG(Animation, Warning, "AnimationComponent has a playing animation but the associated Skeleton asset has no such animation (index {})", playbackState.animationIndex);

                playbackState = {};

                continue;
            }

            playbackState.currentTime += delta * playbackState.speed;

            if (playbackState.currentTime > animation->GetLength())
            {
                playbackState.currentTime = 0.0f;

                if (playbackState.loopMode == AnimationLoopMode::ONCE)
                {
                    playbackState.status = AnimationPlaybackStatus::STOPPED;
                    playbackState.currentTime = 0.0f;
                }
            }

            animation->ApplyBlended(meshComponent.skeleton, playbackState.currentTime, 0.5f);

            meshComponent.skeleton->SetNeedsRenderProxyUpdate();
        }
    }
}

} // namespace hyperion
