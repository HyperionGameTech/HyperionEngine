/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/AnimationSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Animation.hpp>

namespace hyperion {

void AnimationSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, animation_component, mesh_component] : GetEntityManager().GetEntitySet<AnimationComponent, MeshComponent>().GetScopedView(GetComponentInfos())) {
        if (!mesh_component.skeleton) {
            continue;
        }

        AnimationPlaybackState &playback_state = animation_component.playback_state;

        if (playback_state.status == ANIMATION_PLAYBACK_STATUS_PLAYING) {
            if (playback_state.animation_index == ~0u) {
                playback_state = { };

                continue;
            }

            const Handle<Animation> &animation = mesh_component.skeleton->GetAnimation(playback_state.animation_index);
            AssertThrow(animation.IsValid());

            playback_state.current_time += delta * playback_state.speed;

            if (playback_state.current_time > animation->GetLength()) {
                playback_state.current_time = 0.0f;

                if (playback_state.loop_mode == ANIMATION_LOOP_MODE_ONCE) {
                    playback_state.status = ANIMATION_PLAYBACK_STATUS_STOPPED;
                    playback_state.current_time = 0.0f;
                }
            }

            animation->ApplyBlended(playback_state.current_time, 0.5f);

            // mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        }

        mesh_component.skeleton->Update(delta);
    }
}

} // namespace hyperion