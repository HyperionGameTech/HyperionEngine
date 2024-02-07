#ifndef HYPERION_V2_ECS_ANIMATION_COMPONENT_HPP
#define HYPERION_V2_ECS_ANIMATION_COMPONENT_HPP

#include <core/Handle.hpp>
#include <scene/animation/Skeleton.hpp>

namespace hyperion::v2 {

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
    UInt                    animation_index = ~0u;
    AnimationPlaybackStatus status = ANIMATION_PLAYBACK_STATUS_STOPPED;
    AnimationLoopMode       loop_mode = ANIMATION_LOOP_MODE_ONCE;
    Float                   speed = 1.0f;
    Float                   current_time = 0.0f;
};

struct AnimationComponent
{
    AnimationPlaybackState  playback_state;
};

} // namespace hyperion::v2

#endif