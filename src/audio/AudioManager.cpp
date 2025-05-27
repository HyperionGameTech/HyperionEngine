/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <audio/AudioManager.hpp>

#include <cstring>
#include <iostream>

namespace hyperion {

AudioManager& AudioManager::GetInstance()
{
    static AudioManager instance;

    return instance;
}

AudioManager::AudioManager()
    : m_is_initialized(false)
{
}

AudioManager::~AudioManager()
{
    if (m_is_initialized)
    {
        Shutdown();
    }
}

bool AudioManager::Initialize()
{
    m_device = alcOpenDevice(NULL);
    if (!m_device)
    {
        std::cout << "Failed to open OpenAL device\n";
        m_is_initialized = false;
        return false;
    }

    m_context = alcCreateContext(m_device, NULL);
    alcMakeContextCurrent(m_context);
    if (!m_context)
    {
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

void AudioManager::Shutdown()
{
    if (m_is_initialized)
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
    }

    m_is_initialized = false;
}

Array<String> AudioManager::ListDevices() const
{
    Array<String> devices;

    const ALCchar* device = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar* next = device + 1;
    SizeType len = 0;

    while (device && *device != '\0' && next && *next != '\0')
    {
        devices.PushBack(device);
        len = std::strlen(device);
        device += (len + 1);
        next += (len + 2);
    }

    return devices;
}

void AudioManager::SetListenerPosition(const Vec3f& position)
{
    alListener3f(AL_POSITION, position.x, position.y, position.z);
}

void AudioManager::SetListenerOrientation(
    const Vec3f& forward,
    const Vec3f& up)
{
    const float values[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, values);
}
} // namespace hyperion
