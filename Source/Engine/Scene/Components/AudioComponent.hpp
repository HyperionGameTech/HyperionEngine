/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Audio/AudioSource.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

HYP_STRUCT()
struct AudioPlaybackState
{
    HYP_STRUCT_BODY(AudioPlaybackState);

    HYP_FIELD(Property = "Status", Editor = true)
    AudioPlaybackStatus status = AudioPlaybackStatus::Stopped;

    HYP_FIELD(Property = "LoopMode", Editor = true)
    AudioLoopMode loopMode = AudioLoopMode::Once;

    HYP_FIELD(Property = "Speed", Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Editor = true)
    float currentTime = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(status);
        hashCode.Add(loopMode);
        hashCode.Add(speed);
        hashCode.Add(currentTime);

        return hashCode;
    }
};

HYP_STRUCT(Component, Label = "Audio Component", Description = "Controls the state of an audio source.", Editor = true)
struct AudioComponent
{
    HYP_STRUCT_BODY(AudioComponent);

    HYP_FIELD(Property = "AudioSource", Editor = true)
    Handle<AudioSource> audioSource;

    HYP_FIELD(Property = "PlaybackState", Editor = true)
    AudioPlaybackState playbackState;

    HYP_FIELD(Transient)
    Vec3f lastPosition;

    HYP_FIELD(Transient)
    float timer = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(audioSource);
        hashCode.Add(playbackState);

        return hashCode;
    }
};

} // namespace Hyperion
