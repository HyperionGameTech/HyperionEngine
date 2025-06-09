/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/AudioSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <audio/AudioManager.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

void AudioSystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    AudioComponent& audio_component = GetEntityManager().GetComponent<AudioComponent>(entity);

    if (audio_component.audio_source.IsValid())
    {
        InitObject(audio_component.audio_source);

        audio_component.flags |= AUDIO_COMPONENT_FLAG_INIT;
    }
}

void AudioSystem::Process(float delta)
{
    if (!AudioManager::GetInstance().IsInitialized())
    {
        return;
    }

    if (GetEntityManager().GetScene()->IsAudioListener())
    {
        const Handle<Camera>& camera = GetEntityManager().GetScene()->GetPrimaryCamera();

        if (camera.IsValid())
        {
            AudioManager::GetInstance().SetListenerOrientation(camera->GetDirection(), camera->GetUpVector());
            AudioManager::GetInstance().SetListenerPosition(camera->GetTranslation());
        }
    }

    for (auto [entity_id, audio_component, transform_component] : GetEntityManager().GetEntitySet<AudioComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!audio_component.audio_source.IsValid())
        {
            audio_component.playback_state.status = AUDIO_PLAYBACK_STATUS_STOPPED;
            audio_component.playback_state.current_time = 0.0f;

            continue;
        }

        if (audio_component.playback_state.status == AUDIO_PLAYBACK_STATUS_PLAYING)
        {
            switch (audio_component.playback_state.loop_mode)
            {
            case AUDIO_LOOP_MODE_ONCE:
                if (audio_component.playback_state.current_time > audio_component.audio_source->GetDuration())
                {
                    audio_component.playback_state.status = AUDIO_PLAYBACK_STATUS_STOPPED;
                    audio_component.playback_state.current_time = 0.0f;

                    audio_component.audio_source->Stop();
                }

                continue;

                break;
            case AUDIO_LOOP_MODE_REPEAT:
                if (audio_component.playback_state.current_time > audio_component.audio_source->GetDuration())
                {
                    audio_component.playback_state.current_time = 0.0f;
                }

                break;
            }

            audio_component.playback_state.current_time += delta * audio_component.playback_state.speed;

            switch (audio_component.audio_source->GetState())
            {
            case AudioSourceState::PLAYING:
                break;
            case AudioSourceState::PAUSED: // fallthrough
            case AudioSourceState::STOPPED:
                audio_component.audio_source->SetPitch(audio_component.playback_state.speed);
                audio_component.audio_source->SetLoop(audio_component.playback_state.loop_mode == AUDIO_LOOP_MODE_REPEAT);

                audio_component.audio_source->Play();
                break;
            default:
                break;
            }

            const Vec3f& position = transform_component.transform.GetTranslation();

            if (!MathUtil::ApproxEqual(position, audio_component.last_position))
            {
                const Vec3f position_change = position - audio_component.last_position;
                const GameCounter::TickUnit time_change = (audio_component.timer + delta) - audio_component.timer;
                const Vec3f velocity = position_change / time_change;

                audio_component.audio_source->SetPosition(position);
                audio_component.audio_source->SetVelocity(velocity);

                audio_component.last_position = position;
            }
        }
        else if (audio_component.playback_state.status == AUDIO_PLAYBACK_STATUS_PAUSED)
        {
            if (audio_component.audio_source->GetState() != AudioSourceState::PAUSED)
            {
                audio_component.audio_source->Pause();
            }
        }
        else if (audio_component.playback_state.status == AUDIO_PLAYBACK_STATUS_STOPPED)
        {
            if (audio_component.audio_source->GetState() != AudioSourceState::STOPPED)
            {
                audio_component.audio_source->Stop();
            }
        }

        audio_component.timer += delta; // @TODO: prevent overflow
    }
}

} // namespace hyperion
