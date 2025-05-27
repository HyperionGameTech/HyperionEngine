/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ANIMATION_COMPONENT_HPP
#define HYPERION_ECS_ANIMATION_COMPONENT_HPP

#include <HashCode.hpp>

namespace hyperion {

enum class AnimationPlaybackStatus : uint32
{
    STOPPED = 0,
    PAUSED,
    PLAYING
};

enum class AnimationLoopMode : uint32
{
    ONCE = 0,
    REPEAT
};

HYP_STRUCT()

struct AnimationPlaybackState
{
    HYP_FIELD(Property = "AnimationIndex", Serialize = true, Editor = true)
    uint32 animation_index = ~0u;

    HYP_FIELD(Property = "Status", Serialize = true, Editor = true)
    AnimationPlaybackStatus status = AnimationPlaybackStatus::STOPPED;

    HYP_FIELD(Property = "LoopMode", Serialize = true, Editor = true)
    AnimationLoopMode loop_mode = AnimationLoopMode::ONCE;

    HYP_FIELD(Property = "Speed", Serialize = true, Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Serialize = true, Editor = true)
    float current_time = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(animation_index);
        hc.Add(status);
        hc.Add(loop_mode);
        hc.Add(speed);
        hc.Add(current_time);

        return hc;
    }
};

HYP_STRUCT(Component)

struct AnimationComponent
{
    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AnimationPlaybackState playback_state;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(playback_state);

        return hc;
    }
};

} // namespace hyperion

#endif