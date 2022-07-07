#include "AudioManager.hpp"

#include <iostream>
#include <cstring>

namespace hyperion {
AudioManager *AudioManager::instance = nullptr;

AudioManager *AudioManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new AudioManager();
    }
    return instance;
}

AudioManager::AudioManager()
{
    m_is_initialized = false;
}

AudioManager::~AudioManager()
{
    if (m_is_initialized) {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
    }
    m_is_initialized = false;
}

void AudioManager::Deinitialize()
{
    delete instance;
    instance = nullptr;

}

bool AudioManager::Initialize()
{
    m_device = alcOpenDevice(NULL);
    if (!m_device) {
        std::cout << "Failed to open OpenAL device\n";
        m_is_initialized = false;
        return false;
    }

    m_context = alcCreateContext(m_device, NULL);
    alcMakeContextCurrent(m_context);
    if (!m_context) {
        std::cout << "Failed to open OpenAL context\n";
        m_is_initialized = false;
        return false;
    }

    ALfloat orientation[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    alListenerfv(AL_ORIENTATION, orientation);

    m_is_initialized = true;
    return true;
}

void AudioManager::ListDevices()
{
    const ALCchar *device = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar *next = device + 1;
    size_t len = 0;

    std::cout << "Devices list:\n";
    std::cout << "----------\n";
    while (device && *device != '\0' && next && *next != '\0') {
        std::cout << device << "\n";
        len = std::strlen(device);
        device += (len + 1);
        next += (len + 2);
    }
    std::cout << "----------\n";
}

void AudioManager::SetListenerPosition(const Vector3 &position)
{
    alListener3f(AL_POSITION, position.x, position.y, position.z);
}

void AudioManager::SetListenerOrientation(const Vector3 &forward, const Vector3 &up)
{
    const float values[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, values);
}
} // namespace hyperion
