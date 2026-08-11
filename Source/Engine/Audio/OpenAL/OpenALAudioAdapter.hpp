/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Audio/AudioAdapter.hpp>

namespace Hyperion {

class OpenALAudioAdapter : public AudioAdapter<OpenALAudioAdapter>
{
public:
    OpenALAudioAdapter();
    ~OpenALAudioAdapter();

    void Init(AudioManager* audioManager);
    void Teardown(AudioManager* audioManager);

    Array<String> ListDevices() const;

    void SetListenerPosition(const Vec3f& position);
    void SetListenerOrientation(const Vec3f& forward, const Vec3f& up);

    void OnAudioSourceInit(AudioSource* audioSource);
    void OnAudioSourceDestroy(AudioSource* audioSource);
    void OnAudioSourceSoundChanged(AudioSource* audioSource);

    AudioSourceState GetAudioSourceState(const AudioSource* audioSource) const;

    void SetAudioSourcePosition(AudioSource* audioSource, const Vec3f& position);
    void SetAudioSourceVelocity(AudioSource* audioSource, const Vec3f& velocity);
    void SetAudioSourcePitch(AudioSource* audioSource, float pitch);
    void SetAudioSourceGain(AudioSource* audioSource, float gain);
    void SetAudioSourceLoop(AudioSource* audioSource, bool loop);

    void PlayAudioSource(AudioSource* audioSource);
    void PauseAudioSource(AudioSource* audioSource);
    void StopAudioSource(AudioSource* audioSource);

private:
    void* m_device;
    void* m_context;
};

} // namespace Hyperion
