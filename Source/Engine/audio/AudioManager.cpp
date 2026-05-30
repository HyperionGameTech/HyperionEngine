/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <audio/AudioManager.hpp>

#include <engine/EngineGlobals.hpp>

#include <AudioManager.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Audio);

const Handle<AudioManager>& AudioManager::GetInstance()
{
    return g_audioManager;
}

AudioManager::AudioManager()
    : m_isInitialized(false)
{
}

AudioManager::~AudioManager()
{
    if (m_isInitialized)
    {
        Shutdown();
    }
}

void AudioManager::Initialize()
{
    if (m_isInitialized)
    {
        return;
    }

#if HYP_OPENAL
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
#endif

    m_isInitialized = true;
}

void AudioManager::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

#if HYP_OPENAL
    alcMakeContextCurrent(NULL);
    alcDestroyContext(m_context);
    alcCloseDevice(m_device);
#endif

    m_isInitialized = false;
}

Array<String> AudioManager::ListDevices() const
{
    Array<String> devices;

#if HYP_OPENAL
    const ALCchar* device = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar* next = device + 1;
    size_t len = 0;

    while (device && *device != '\0' && next && *next != '\0')
    {
        devices.PushBack(device);
        len = std::strlen(device);
        device += (len + 1);
        next += (len + 2);
    }
#endif

    return devices;
}

void AudioManager::SetListenerPosition(const Vec3f& position)
{
#if HYP_OPENAL
    alListener3f(AL_POSITION, position.x, position.y, position.z);
#endif
}

void AudioManager::SetListenerOrientation(
    const Vec3f& forward,
    const Vec3f& up)
{
#if HYP_OPENAL
    const float values[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, values);
#endif
}
} // namespace Hyperion
