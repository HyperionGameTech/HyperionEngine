#include "AudioSource.hpp"
#include "AudioManager.hpp"

namespace hyperion {
AudioSource::AudioSource(Format format, const unsigned char *data, size_t size, size_t freq)
    : m_format(format),
      m_buffer_id(~0u),
      m_source_id(~0u),
      m_sample_length(0)
{
    if (!AudioManager::GetInstance()->IsInitialized()) {
        return;
    }

    auto al_format = AL_FORMAT_MONO8;

    switch (format) {
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
    alBufferData(m_buffer_id, al_format, (void*)data, size, freq);

    alGenSources(1, &m_source_id);
    alSourcei(m_source_id, AL_BUFFER, m_buffer_id);

    FindSampleLength();
}

AudioSource::~AudioSource()
{
    Stop();

    if (m_source_id != ~0u) {
        alDeleteSources(1, &m_source_id);
    }

    if (m_buffer_id != ~0u) {
        alDeleteBuffers(1, &m_buffer_id);
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
    default:         return State::UNDEFINED;
    }
}


void AudioSource::SetPosition(const Vector3 &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(m_source_id, AL_POSITION, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetVelocity(const Vector3 &vec)
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

} // namespace hyperion
