#ifndef HYPERION_V2_ECS_AUDIO_COMPONENT_HPP
#define HYPERION_V2_ECS_AUDIO_COMPONENT_HPP

#include <audio/AudioSource.hpp>
#include <core/Handle.hpp>
#include <HashCode.hpp>
#include <GameCounter.hpp>

namespace hyperion::v2 {

using AudioComponentFlags = UInt32;

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
    Float               speed = 1.0f;
    Float               current_time = 0.0f;
};

struct AudioComponent
{
    Handle<AudioSource>     audio_source;
    AudioPlaybackState      playback_state;

    AudioComponentFlags     flags = AUDIO_COMPONENT_FLAG_NONE;

    Vec3f                   last_position;
    GameCounter::TickUnit   timer = 0.0f;
};

} // namespace hyperion::v2

#endif