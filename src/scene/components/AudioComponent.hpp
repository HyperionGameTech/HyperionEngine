/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <audio/AudioSource.hpp>

#include <core/object/Handle.hpp>
#include <core/object/HypObject.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

HYP_ENUM()
enum class AudioComponentFlags : uint8
{
    NONE = 0x0,
    INIT = 0x1
};

HYP_MAKE_ENUM_FLAGS(AudioComponentFlags);

HYP_ENUM()
enum AudioPlaybackStatus : uint8
{
    APS_STOPPED,
    APS_PAUSED,
    APS_PLAYING
};

HYP_ENUM()
enum AudioLoopMode : uint8
{
    ALM_ONCE,
    ALM_REPEAT
};

HYP_STRUCT()
struct AudioPlaybackState
{
    HYP_FIELD(Property = "Status", Serialize = true, Editor = true)
    AudioPlaybackStatus status = APS_STOPPED;

    HYP_FIELD(Property = "LoopMode", Serialize = true, Editor = true)
    AudioLoopMode loopMode = ALM_ONCE;

    HYP_FIELD(Property = "Speed", Serialize = true, Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Serialize = true, Editor = true)
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
    HYP_FIELD(Property = "AudioSource", Serialize = true, Editor = true)
    Handle<AudioSource> audioSource;

    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AudioPlaybackState playbackState;

    HYP_FIELD()
    EnumFlags<AudioComponentFlags> flags = AudioComponentFlags::NONE;

    HYP_FIELD()
    Vec3f lastPosition;

    HYP_FIELD()
    float timer = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(audioSource);
        hashCode.Add(playbackState);

        return hashCode;
    }
};

} // namespace hyperion
