/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_AUDIO_COMPONENT_HPP
#define HYPERION_ECS_AUDIO_COMPONENT_HPP

#include <audio/AudioSource.hpp>
#include <core/Handle.hpp>

#include <HashCode.hpp>
#include <GameCounter.hpp>

namespace hyperion {

using AudioComponentFlags = uint32;

enum AudioComponentFlagBits : AudioComponentFlags
{
    AUDIO_COMPONENT_FLAG_NONE = 0x0,
    AUDIO_COMPONENT_FLAG_INIT = 0x1,
};

enum AudioPlaybackStatus
{
    AUDIO_PLAYBACK_STATUS_STOPPED,
    AUDIO_PLAYBACK_STATUS_PAUSED,
    AUDIO_PLAYBACK_STATUS_PLAYING
};

enum AudioLoopMode
{
    AUDIO_LOOP_MODE_ONCE,
    AUDIO_LOOP_MODE_REPEAT
};

HYP_STRUCT()

struct AudioPlaybackState
{
    HYP_FIELD(Property = "Status", Serialize = true, Editor = true)
    AudioPlaybackStatus status = AUDIO_PLAYBACK_STATUS_STOPPED;

    HYP_FIELD(Property = "LoopMode", Serialize = true, Editor = true)
    AudioLoopMode loop_mode = AUDIO_LOOP_MODE_ONCE;

    HYP_FIELD(Property = "Speed", Serialize = true, Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Serialize = true, Editor = true)
    float current_time = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;
        hash_code.Add(status);
        hash_code.Add(loop_mode);
        hash_code.Add(speed);
        hash_code.Add(current_time);

        return hash_code;
    }
};

HYP_STRUCT(Component, Label = "Audio Component", Description = "Controls the state of an audio source.", Editor = true)

struct AudioComponent
{
    HYP_FIELD(Property = "AudioSource", Serialize = true, Editor = true)
    Handle<AudioSource> audio_source;

    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AudioPlaybackState playback_state;

    HYP_FIELD()
    AudioComponentFlags flags = AUDIO_COMPONENT_FLAG_NONE;

    HYP_FIELD()
    Vec3f last_position;

    HYP_FIELD()
    float timer = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;
        hash_code.Add(audio_source);
        hash_code.Add(playback_state);

        return hash_code;
    }
};

} // namespace hyperion

#endif