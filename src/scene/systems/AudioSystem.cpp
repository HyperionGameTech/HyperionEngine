/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/AudioSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <audio/AudioManager.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

void AudioSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    AudioComponent& audioComponent = GetEntityManager().GetComponent<AudioComponent>(entity);

    if (audioComponent.audioSource.IsValid())
    {
        InitObject(audioComponent.audioSource);

        audioComponent.flags |= AUDIO_COMPONENT_FLAG_INIT;
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
        if (Camera* camera = GetEntityManager().GetScene()->GetPrimaryCamera())
        {
            AudioManager::GetInstance().SetListenerOrientation(camera->GetDirection(), camera->GetUpVector());
            AudioManager::GetInstance().SetListenerPosition(camera->GetWorldTranslation());
        }
    }

    for (auto [entity, audioComponent, transformComponent] : GetEntityManager().GetEntitySet<AudioComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!audioComponent.audioSource.IsValid())
        {
            audioComponent.playbackState.status = AUDIO_PLAYBACK_STATUS_STOPPED;
            audioComponent.playbackState.currentTime = 0.0f;

            continue;
        }

        if (audioComponent.playbackState.status == AUDIO_PLAYBACK_STATUS_PLAYING)
        {
            switch (audioComponent.playbackState.loopMode)
            {
            case AUDIO_LOOP_MODE_ONCE:
                if (audioComponent.playbackState.currentTime > audioComponent.audioSource->GetDuration())
                {
                    audioComponent.playbackState.status = AUDIO_PLAYBACK_STATUS_STOPPED;
                    audioComponent.playbackState.currentTime = 0.0f;

                    audioComponent.audioSource->Stop();
                }

                continue;

                break;
            case AUDIO_LOOP_MODE_REPEAT:
                if (audioComponent.playbackState.currentTime > audioComponent.audioSource->GetDuration())
                {
                    audioComponent.playbackState.currentTime = 0.0f;
                }

                break;
            }

            audioComponent.playbackState.currentTime += delta * audioComponent.playbackState.speed;

            switch (audioComponent.audioSource->GetState())
            {
            case AudioSourceState::PLAYING:
                break;
            case AudioSourceState::PAUSED: // fallthrough
            case AudioSourceState::STOPPED:
                audioComponent.audioSource->SetPitch(audioComponent.playbackState.speed);
                audioComponent.audioSource->SetLoop(audioComponent.playbackState.loopMode == AUDIO_LOOP_MODE_REPEAT);

                audioComponent.audioSource->Play();
                break;
            default:
                break;
            }

            const Vec3f& position = transformComponent.transform.GetTranslation();

            if (!MathUtil::ApproxEqual(position, audioComponent.lastPosition))
            {
                const Vec3f positionChange = position - audioComponent.lastPosition;
                const float timeChange = (audioComponent.timer + delta) - audioComponent.timer;
                const Vec3f velocity = positionChange / timeChange;

                audioComponent.audioSource->SetPosition(position);
                audioComponent.audioSource->SetVelocity(velocity);

                audioComponent.lastPosition = position;
            }
        }
        else if (audioComponent.playbackState.status == AUDIO_PLAYBACK_STATUS_PAUSED)
        {
            if (audioComponent.audioSource->GetState() != AudioSourceState::PAUSED)
            {
                audioComponent.audioSource->Pause();
            }
        }
        else if (audioComponent.playbackState.status == AUDIO_PLAYBACK_STATUS_STOPPED)
        {
            if (audioComponent.audioSource->GetState() != AudioSourceState::STOPPED)
            {
                audioComponent.audioSource->Stop();
            }
        }

        audioComponent.timer += delta; // @TODO: prevent overflow
    }
}

} // namespace hyperion
