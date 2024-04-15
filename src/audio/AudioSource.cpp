/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "AudioSource.hpp"
#include "AudioManager.hpp"

#include <Engine.hpp>

namespace hyperion::v2 {

AudioSource::AudioSource(Format format, const ByteBuffer &byte_buffer, SizeType freq)
    : BasicObject(),
      m_format(format),
      m_data(byte_buffer),
      m_freq(freq),
      m_buffer_id(~0u),
      m_source_id(~0u),
      m_sample_length(0)
{
}

AudioSource::~AudioSource()
{
    Stop();

    if (m_source_id != ~0u) {
        alDeleteSources(1, &m_source_id);
        m_source_id = ~0u;
    }

    if (m_buffer_id != ~0u) {
        alDeleteBuffers(1, &m_buffer_id);
        m_buffer_id = ~0u;
    }
}

void AudioSource::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    if (AudioManager::GetInstance()->IsInitialized()) {
        auto al_format = AL_FORMAT_MONO8;

        switch (m_format) {
        case Format::MONO8:
            al_format = AL_FORMAT_MONO8;
            break;
        case Format::MONO16:
            al_format = AL_FORMAT_MONO16;
            break;
        case Format::STEREO8:
            al_format = AL_FORMAT_STEREO8;
            break;
        case Format::STEREO16:
            al_format = AL_FORMAT_STEREO16;
            break;
        }

        alGenBuffers(1, &m_buffer_id);
        alBufferData(m_buffer_id, al_format, m_data.Data(), m_data.Size(), m_freq);

        alGenSources(1, &m_source_id);
        alSourcei(m_source_id, AL_BUFFER, m_buffer_id);

        FindSampleLength();

        // drop reference
        m_data = ByteBuffer();
    }
}

AudioSource::State AudioSource::GetState() const
{
    if (!AudioManager::GetInstance()->IsInitialized()) {
        return State::UNDEFINED;
    }

    ALint state;
    alGetSourcei(m_source_id, AL_SOURCE_STATE, &state);

    switch (state) {
    case AL_INITIAL: // fallthrough
    case AL_STOPPED: return State::STOPPED;
    case AL_PLAYING: return State::PLAYING;
    case AL_PAUSED:  return State::PAUSED;
    default: return State::UNDEFINED;
    }
}


void AudioSource::SetPosition(const Vec3f &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(m_source_id, AL_POSITION, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetVelocity(const Vec3f &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(m_source_id, AL_VELOCITY, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetPitch(float pitch)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcef(m_source_id, AL_PITCH, pitch);
    }
}

void AudioSource::SetGain(float gain)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcef(m_source_id, AL_GAIN, gain);
    }
}

void AudioSource::SetLoop(bool loop)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcei(m_source_id, AL_LOOPING, loop);
    }
}

void AudioSource::Play()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcePlay(m_source_id);
    }
}

void AudioSource::Pause()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcePause(m_source_id);
    }
}

void AudioSource::Stop()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourceStop(m_source_id);
    }
}

void AudioSource::FindSampleLength()
{
    if (!AudioManager::GetInstance()->IsInitialized()) {
        return;
    }

    ALint byte_size;
    ALint num_channels;
    ALint bits;

    alGetBufferi(m_buffer_id, AL_SIZE, &byte_size);
    alGetBufferi(m_buffer_id, AL_CHANNELS, &num_channels);
    alGetBufferi(m_buffer_id, AL_BITS, &bits);

    m_sample_length = byte_size * 8 / (num_channels * bits);
}

} // namespace hyperion::v2
