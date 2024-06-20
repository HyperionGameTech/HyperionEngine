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
    AUDIO_COMPONENT_FLAG_NONE   = 0x0,
    AUDIO_COMPONENT_FLAG_INIT   = 0x1,
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

struct AudioPlaybackState
{
    AudioPlaybackStatus status = AUDIO_PLAYBACK_STATUS_STOPPED;
    AudioLoopMode       loop_mode = AUDIO_LOOP_MODE_ONCE;
    float               speed = 1.0f;
    float               current_time = 0.0f;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(status);
        hash_code.Add(loop_mode);
        hash_code.Add(speed);
        hash_code.Add(current_time);

        return hash_code;
    }
};

struct AudioComponent
{
    Handle<AudioSource>     audio_source;
    AudioPlaybackState      playback_state;

    AudioComponentFlags     flags = AUDIO_COMPONENT_FLAG_NONE;

    Vec3f                   last_position;
    GameCounter::TickUnit   timer = 0.0f;

    HYP_NODISCARD HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(audio_source);
        hash_code.Add(playback_state);

        return hash_code;
    }
};

} // namespace hyperion

#endif