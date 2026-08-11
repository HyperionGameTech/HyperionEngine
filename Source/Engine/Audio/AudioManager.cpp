/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Audio/AudioManager.hpp>

#include <Framework/EngineGlobals.hpp>

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

    m_adapter.Init(this);

    m_isInitialized = true;
}

void AudioManager::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    m_adapter.Teardown(this);

    m_isInitialized = false;
}

Array<String> AudioManager::ListDevices() const
{
    return m_adapter.ListDevices();
}

void AudioManager::SetListenerPosition(const Vec3f& position)
{
    m_adapter.SetListenerPosition(position);
}

void AudioManager::SetListenerOrientation(
    const Vec3f& forward,
    const Vec3f& up)
{
    m_adapter.SetListenerOrientation(forward, up);
}
} // namespace Hyperion
