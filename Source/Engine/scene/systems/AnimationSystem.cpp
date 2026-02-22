/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>

#include <scene/systems/AnimationSystem.hpp>

#include <scene/animation/Animation.hpp>
#include <scene/animation/Skeleton.hpp>

#include <Core/reflection/Handle.hpp>

#include <engine/GameState.hpp>

#include <AnimationSystem.generated.inl>

namespace Hyperion {

bool AnimationSystem::ShouldProcessScene(Scene* scene) const
{
    return !(scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED));
}

void AnimationSystem::OnEntityAdded(Entity* entity)
{
    HYP_SCOPE;

    SystemBase::OnEntityAdded(entity);

    const MeshComponent& meshComponent = entity->GetEntityManager()->GetComponent<MeshComponent>(entity);
    InitObject(meshComponent.skeleton);

    if (meshComponent.skeleton.IsValid())
    {
        auto& locks = m_resourceHandles[meshComponent.skeleton.Get()];

        for (const Handle<Animation>& anim : meshComponent.skeleton->GetAnimations())
        {
            for (const Handle<AnimationTrack>& track : anim->GetTracks())
            {
                locks.PushBack(MakeUnique<TSharedLock<AssetObject>>(*track));
            }
        }
    }

    AnimationComponent& animationComponent = entity->GetEntityManager()->GetComponent<AnimationComponent>(entity);
    animationComponent.playbackState = {
        .animationIndex = 0,
        .status = AnimationPlaybackStatus::PLAYING,
        .loopMode = AnimationLoopMode::REPEAT,
        .speed = 1.0f,
        .currentTime = 0.0f
    };
}

void AnimationSystem::OnEntityRemoved(Entity* entity)
{
    HYP_SCOPE;

    SystemBase::OnEntityRemoved(entity);

    const AnimationComponent& animationComponent = entity->GetEntityManager()->GetComponent<AnimationComponent>(entity);
    const MeshComponent& meshComponent = entity->GetEntityManager()->GetComponent<MeshComponent>(entity);

    if (meshComponent.skeleton.IsValid())
    {
        auto it = m_resourceHandles.Find(meshComponent.skeleton.Get());
        if (it != m_resourceHandles.End())
        {
            m_resourceHandles.Erase(it);
        }
    }
}

void AnimationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!GetWorld()->GetGameState().IsSimulating())
    {
        // return;
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
}

} // namespace Hyperion
