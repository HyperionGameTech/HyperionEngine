/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ANIMATION_COMPONENT_HPP
#define HYPERION_ECS_ANIMATION_COMPONENT_HPP

#include <core/Handle.hpp>
#include <scene/animation/Skeleton.hpp>

#include <HashCode.hpp>

namespace hyperion {

enum AnimationPlaybackStatus
{
    ANIMATION_PLAYBACK_STATUS_STOPPED,
    ANIMATION_PLAYBACK_STATUS_PAUSED,
    ANIMATION_PLAYBACK_STATUS_PLAYING
};

enum AnimationLoopMode
{
    ANIMATION_LOOP_MODE_ONCE,
    ANIMATION_LOOP_MODE_REPEAT
};

struct AnimationPlaybackState
{
    uint                    animation_index = ~0u;
    AnimationPlaybackStatus status = ANIMATION_PLAYBACK_STATUS_STOPPED;
    AnimationLoopMode       loop_mode = ANIMATION_LOOP_MODE_ONCE;
    float                   speed = 1.0f;
    float                   current_time = 0.0f;

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

HYP_STRUCT()
struct AnimationComponent
{
    HYP_FIELD(SerializeAs=PlaybackState)
    AnimationPlaybackState  playback_state;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(playback_state);

        return hc;
    }
};

} // namespace hyperion

#endif