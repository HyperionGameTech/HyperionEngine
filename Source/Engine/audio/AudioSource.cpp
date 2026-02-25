/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <audio/AudioSource.hpp>
#include <audio/AudioManager.hpp>

#include <engine/EngineGlobals.hpp>

#include <AudioSource.generated.inl>

namespace Hyperion {

AudioSource::AudioSource()
    : ObjectBase(),
      m_format(AudioSourceFormat::MONO8),
      m_freq(0),
      m_bufferId(~0u),
      m_sourceId(~0u),
      m_sampleLength(0)
{
}

AudioSource::AudioSource(AudioSourceFormat format, const ByteBuffer& byteBuffer, uint64 freq)
    : ObjectBase(),
      m_format(format),
      m_data(byteBuffer),
      m_freq(freq),
      m_bufferId(~0u),
      m_sourceId(~0u),
      m_sampleLength(0)
{
}

AudioSource::AudioSource(AudioSource&& other) noexcept
    : ObjectBase(),
      m_format(other.m_format),
      m_freq(other.m_freq),
      m_data(std::move(other.m_data)),
      m_bufferId(other.m_bufferId),
      m_sourceId(other.m_sourceId),
      m_sampleLength(other.m_sampleLength)
{
    other.m_format = AudioSourceFormat::MONO8;
    other.m_freq = 0;
    other.m_bufferId = ~0u;
    other.m_sourceId = ~0u;
    other.m_sampleLength = 0;
}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_format = other.m_format;
    m_freq = other.m_freq;
    m_data = std::move(other.m_data);
    m_bufferId = other.m_bufferId;
    m_sourceId = other.m_sourceId;
    m_sampleLength = other.m_sampleLength;

    other.m_format = AudioSourceFormat::MONO8;
    other.m_freq = 0;
    other.m_bufferId = ~0u;
    other.m_sourceId = ~0u;
    other.m_sampleLength = 0;

    return *this;
}

AudioSource::~AudioSource()
{
    Stop();

    if (m_sourceId != ~0u)
    {
        alDeleteSources(1, &m_sourceId);
        m_sourceId = ~0u;
    }

    if (m_bufferId != ~0u)
    {
        alDeleteBuffers(1, &m_bufferId);
        m_bufferId = ~0u;
    }
}

void AudioSource::Init()
{
    if (g_audioManager->IsReady())
    {
        auto alFormat = AL_FORMAT_MONO8;

        switch (m_format)
        {
        case AudioSourceFormat::MONO8:
            alFormat = AL_FORMAT_MONO8;
            break;
        case AudioSourceFormat::MONO16:
            alFormat = AL_FORMAT_MONO16;
            break;
        case AudioSourceFormat::STEREO8:
            alFormat = AL_FORMAT_STEREO8;
            break;
        case AudioSourceFormat::STEREO16:
            alFormat = AL_FORMAT_STEREO16;
            break;
        }

        alGenBuffers(1, &m_bufferId);
        alBufferData(m_bufferId, alFormat, m_data.Data(), m_data.Size(), m_freq);

        alGenSources(1, &m_sourceId);
        alSourcei(m_sourceId, AL_BUFFER, m_bufferId);

        FindSampleLength();

        // drop reference
        m_data = ByteBuffer();
        
        SetReady(true);
    }
}

AudioSourceState AudioSource::GetState() const
{
    if (!g_audioManager->IsReady())
    {
        return AudioSourceState::UNDEFINED;
    }

    ALint state;
    alGetSourcei(m_sourceId, AL_SOURCE_STATE, &state);

    switch (state)
    {
    case AL_INITIAL: // fallthrough
    case AL_STOPPED:
        return AudioSourceState::STOPPED;
    case AL_PLAYING:
        return AudioSourceState::PLAYING;
    case AL_PAUSED:
        return AudioSourceState::PAUSED;
    default:
        return AudioSourceState::UNDEFINED;
    }
}

void AudioSource::SetPosition(const Vec3f& vec)
{
    if (g_audioManager->IsReady())
    {
        alSource3f(m_sourceId, AL_POSITION, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetVelocity(const Vec3f& vec)
{
    if (g_audioManager->IsReady())
    {
        alSource3f(m_sourceId, AL_VELOCITY, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetPitch(float pitch)
{
    if (g_audioManager->IsReady())
    {
        alSourcef(m_sourceId, AL_PITCH, pitch);
    }
}

void AudioSource::SetGain(float gain)
{
    if (g_audioManager->IsReady())
    {
        alSourcef(m_sourceId, AL_GAIN, gain);
    }
}

void AudioSource::SetLoop(bool loop)
{
    if (g_audioManager->IsReady())
    {
        alSourcei(m_sourceId, AL_LOOPING, loop);
    }
}

void AudioSource::Play()
{
    if (g_audioManager->IsReady())
    {
        alSourcePlay(m_sourceId);
    }
}

void AudioSource::Pause()
{
    if (g_audioManager->IsReady())
    {
        alSourcePause(m_sourceId);
    }
}

void AudioSource::Stop()
{
    if (g_audioManager->IsReady())
    {
        alSourceStop(m_sourceId);
    }
}

void AudioSource::FindSampleLength()
{
    if (!g_audioManager->IsReady())
    {
        return;
    }

    ALint byteSize;
    ALint numChannels;
    ALint bits;

    alGetBufferi(m_bufferId, AL_SIZE, &byteSize);
    alGetBufferi(m_bufferId, AL_CHANNELS, &numChannels);
    alGetBufferi(m_bufferId, AL_BITS, &bits);

    m_sampleLength = byteSize * 8 / (numChannels * bits);
}

} // namespace Hyperion
