/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/AudioSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/camera/Camera.hpp>

#include <audio/AudioManager.hpp>

#include <core/math/MathUtil.hpp>

#include <AudioSystem.generated.inl>

namespace hyperion {

bool AudioSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void AudioSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    AudioComponent& audioComponent = entity->GetEntityManager()->GetComponent<AudioComponent>(entity);

    if (audioComponent.audioSource.IsValid())
    {
        InitObject(audioComponent.audioSource);

        audioComponent.flags |= AudioComponentFlags::INIT;
    }
}

void AudioSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!AudioManager::GetInstance().IsInitialized())
    {
        return;
    }

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

        if (scene->GetIsAudioListener())
        {
            if (Camera* camera = scene->GetPrimaryCamera())
            {
                AudioManager::GetInstance().SetListenerOrientation(camera->GetDirection(), camera->GetUpVector());
                AudioManager::GetInstance().SetListenerPosition(camera->GetTranslation());
            }
        }

        for (auto [entity, audioComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<AudioComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!audioComponent.audioSource.IsValid())
            {
                audioComponent.playbackState.status = APS_STOPPED;
                audioComponent.playbackState.currentTime = 0.0f;

                continue;
            }

            if (audioComponent.playbackState.status == APS_PLAYING)
            {
                switch (audioComponent.playbackState.loopMode)
                {
                case ALM_ONCE:
                    if (audioComponent.playbackState.currentTime > audioComponent.audioSource->GetDuration())
                    {
                        audioComponent.playbackState.status = APS_STOPPED;
                        audioComponent.playbackState.currentTime = 0.0f;

                        audioComponent.audioSource->Stop();
                    }

                    continue;

                    break;
                case ALM_REPEAT:
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
                    audioComponent.audioSource->SetLoop(audioComponent.playbackState.loopMode == ALM_REPEAT);

                    audioComponent.audioSource->Play();
                    break;
                default:
                    break;
                }

                const Vec3f& position = transformComponent.translation;

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
            else if (audioComponent.playbackState.status == APS_PAUSED)
            {
                if (audioComponent.audioSource->GetState() != AudioSourceState::PAUSED)
                {
                    audioComponent.audioSource->Pause();
                }
            }
            else if (audioComponent.playbackState.status == APS_STOPPED)
            {
                if (audioComponent.audioSource->GetState() != AudioSourceState::STOPPED)
                {
                    audioComponent.audioSource->Stop();
                }
            }

            audioComponent.timer += delta; // @TODO: prevent overflow
        }
    }
}

} // namespace hyperion
