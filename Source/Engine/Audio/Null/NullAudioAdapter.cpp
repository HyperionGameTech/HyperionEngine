/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Audio/Null/NullAudioAdapter.hpp>

#include <Audio/AudioSource.hpp>

namespace Hyperion {

NullAudioAdapter::NullAudioAdapter() = default;

NullAudioAdapter::~NullAudioAdapter() = default;

void NullAudioAdapter::Init(AudioManager* audioManager)
{
}

void NullAudioAdapter::Teardown(AudioManager* audioManager)
{
}

Array<String> NullAudioAdapter::ListDevices() const
{
    return {};
}

void NullAudioAdapter::SetListenerPosition(const Vec3f& position)
{
}

void NullAudioAdapter::SetListenerOrientation(const Vec3f& forward, const Vec3f& up)
{
}

void NullAudioAdapter::OnAudioSourceInit(AudioSource* audioSource)
{
}

void NullAudioAdapter::OnAudioSourceDestroy(AudioSource* audioSource)
{
}

void NullAudioAdapter::OnAudioSourceSoundChanged(AudioSource* audioSource)
{
}

AudioSourceState NullAudioAdapter::GetAudioSourceState(const AudioSource* audioSource) const
{
    return AudioSourceState::UNDEFINED;
}

void NullAudioAdapter::SetAudioSourcePosition(AudioSource* audioSource, const Vec3f& position)
{
}

void NullAudioAdapter::SetAudioSourceVelocity(AudioSource* audioSource, const Vec3f& velocity)
{
}

void NullAudioAdapter::SetAudioSourcePitch(AudioSource* audioSource, float pitch)
{
}

void NullAudioAdapter::SetAudioSourceGain(AudioSource* audioSource, float gain)
{
}

void NullAudioAdapter::SetAudioSourceLoop(AudioSource* audioSource, bool loop)
{
}

void NullAudioAdapter::PlayAudioSource(AudioSource* audioSource)
{
}

void NullAudioAdapter::PauseAudioSource(AudioSource* audioSource)
{
}

void NullAudioAdapter::StopAudioSource(AudioSource* audioSource)
{
}

} // namespace Hyperion
