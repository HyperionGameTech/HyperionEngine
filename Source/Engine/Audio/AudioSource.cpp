/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Audio/AudioSource.hpp>
#include <Audio/AudioManager.hpp>
#include <Audio/Sound.hpp>

#include <Framework/EngineGlobals.hpp>

#include <AudioSource.generated.inl>

namespace Hyperion {

AudioSource::AudioSource()
    : ObjectBase()
{
}

AudioSource::AudioSource(AudioSource&& other) noexcept
    : ObjectBase(),
      m_sound(std::move(other.m_sound)),
      m_internalData(std::move(other.m_internalData)),
      m_onChanged(std::move(other.m_onChanged))
{
}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_sound = std::move(other.m_sound);
    m_internalData = std::move(other.m_internalData);
    m_onChanged = std::move(other.m_onChanged);

    return *this;
}

AudioSource::~AudioSource()
{
    Stop();

    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().OnAudioSourceDestroy(this);
    }
}

void AudioSource::Init()
{
    ObjectBase::Init();

    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().OnAudioSourceInit(this);
    }

    SetReady(true);
}

void AudioSource::SetSound(const Handle<Sound>& sound)
{
    if (m_sound == sound)
    {
        return;
    }

    m_sound = sound;

    if (IsReady() && g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().OnAudioSourceSoundChanged(this);
    }

    NotifyChanged();
}

AudioSourceState AudioSource::GetState() const
{
    if (!g_audioManager.IsValid() || !g_audioManager->IsInitialized())
    {
        return AudioSourceState::UNDEFINED;
    }

    return g_audioManager->GetAdapter().GetAudioSourceState(this);
}

void AudioSource::SetPosition(const Vec3f& vec)
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().SetAudioSourcePosition(this, vec);
    }

    NotifyChanged();
}

void AudioSource::SetVelocity(const Vec3f& vec)
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().SetAudioSourceVelocity(this, vec);
    }

    NotifyChanged();
}

void AudioSource::SetPitch(float pitch)
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().SetAudioSourcePitch(this, pitch);
    }

    NotifyChanged();
}

void AudioSource::SetGain(float gain)
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().SetAudioSourceGain(this, gain);
    }

    NotifyChanged();
}

void AudioSource::SetLoop(bool loop)
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().SetAudioSourceLoop(this, loop);
    }

    NotifyChanged();
}

void AudioSource::Play()
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().PlayAudioSource(this);
    }
}

void AudioSource::Pause()
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().PauseAudioSource(this);
    }
}

void AudioSource::Stop()
{
    if (g_audioManager.IsValid() && g_audioManager->IsInitialized())
    {
        g_audioManager->GetAdapter().StopAudioSource(this);
    }
}

} // namespace Hyperion
