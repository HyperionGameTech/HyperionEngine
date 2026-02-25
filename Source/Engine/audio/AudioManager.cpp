/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <audio/AudioManager.hpp>

#include <engine/EngineGlobals.hpp>

#include <AudioManager.generated.inl>

namespace Hyperion {

const Handle<AudioManager>& AudioManager::GetInstance()
{
    return g_audioManager;
}

AudioManager::AudioManager()
{
}

AudioManager::~AudioManager()
{
    if (IsReady())
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
    }
}

void AudioManager::Init()
{
    m_device = alcOpenDevice(NULL);
    if (!m_device)
    {
        HYP_LOG(Audio, Error, "Failed to open OpenAL device!");

        return;
    }

    m_context = alcCreateContext(m_device, NULL);
    alcMakeContextCurrent(m_context);
    if (!m_context)
    {
        HYP_LOG(Audio, Error, "Failed to open OpenAL context!");

        return;
    }

    ALfloat orientation[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    alListenerfv(AL_ORIENTATION, orientation);

    SetReady(true);
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
} // namespace Hyperion
