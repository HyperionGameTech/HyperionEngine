/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class AudioManager;
class AudioSource;
class Sound;

enum class AudioSourceState : uint32;

template <class DerivedAdapter>
class AudioAdapter
{
public:
    DerivedAdapter* GetDerivedAdapter()
    {
        return static_cast<DerivedAdapter*>(this);
    }

    const DerivedAdapter* GetDerivedAdapter() const
    {
        return static_cast<const DerivedAdapter*>(this);
    }

    void Init(AudioManager* audioManager)
    {
        GetDerivedAdapter()->DerivedAdapter::Init(audioManager);
    }

    void Teardown(AudioManager* audioManager)
    {
        GetDerivedAdapter()->DerivedAdapter::Teardown(audioManager);
    }

    Array<String> ListDevices() const
    {
        return GetDerivedAdapter()->DerivedAdapter::ListDevices();
    }

    void SetListenerPosition(const Vec3f& position)
    {
        GetDerivedAdapter()->DerivedAdapter::SetListenerPosition(position);
    }

    void SetListenerOrientation(const Vec3f& forward, const Vec3f& up)
    {
        GetDerivedAdapter()->DerivedAdapter::SetListenerOrientation(forward, up);
    }

    void OnAudioSourceInit(AudioSource* audioSource)
    {
        GetDerivedAdapter()->DerivedAdapter::OnAudioSourceInit(audioSource);
    }

    void OnAudioSourceDestroy(AudioSource* audioSource)
    {
        GetDerivedAdapter()->DerivedAdapter::OnAudioSourceDestroy(audioSource);
    }

    void OnAudioSourceSoundChanged(AudioSource* audioSource)
    {
        GetDerivedAdapter()->DerivedAdapter::OnAudioSourceSoundChanged(audioSource);
    }

    AudioSourceState GetAudioSourceState(const AudioSource* audioSource) const
    {
        return GetDerivedAdapter()->DerivedAdapter::GetAudioSourceState(audioSource);
    }

    void SetAudioSourcePosition(AudioSource* audioSource, const Vec3f& position)
    {
        GetDerivedAdapter()->DerivedAdapter::SetAudioSourcePosition(audioSource, position);
    }

    void SetAudioSourceVelocity(AudioSource* audioSource, const Vec3f& velocity)
    {
        GetDerivedAdapter()->DerivedAdapter::SetAudioSourceVelocity(audioSource, velocity);
    }

    void SetAudioSourcePitch(AudioSource* audioSource, float pitch)
    {
        GetDerivedAdapter()->DerivedAdapter::SetAudioSourcePitch(audioSource, pitch);
    }

    void SetAudioSourceGain(AudioSource* audioSource, float gain)
    {
        GetDerivedAdapter()->DerivedAdapter::SetAudioSourceGain(audioSource, gain);
    }

    void SetAudioSourceLoop(AudioSource* audioSource, bool loop)
    {
        GetDerivedAdapter()->DerivedAdapter::SetAudioSourceLoop(audioSource, loop);
    }

    void PlayAudioSource(AudioSource* audioSource)
    {
        GetDerivedAdapter()->DerivedAdapter::PlayAudioSource(audioSource);
    }

    void PauseAudioSource(AudioSource* audioSource)
    {
        GetDerivedAdapter()->DerivedAdapter::PauseAudioSource(audioSource);
    }

    void StopAudioSource(AudioSource* audioSource)
    {
        GetDerivedAdapter()->DerivedAdapter::StopAudioSource(audioSource);
    }
};

} // namespace Hyperion
