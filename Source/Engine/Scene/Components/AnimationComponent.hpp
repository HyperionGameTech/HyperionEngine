/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_ENUM()
enum class AnimationPlaybackStatus : uint8
{
    STOPPED = 0,
    PAUSED,
    PLAYING
};

HYP_ENUM()
enum class AnimationLoopMode : uint8
{
    ONCE = 0,
    REPEAT
};

HYP_STRUCT()
struct AnimationPlaybackState
{
    HYP_STRUCT_BODY(AnimationPlaybackState);

    HYP_FIELD(Property = "AnimationIndex", Editor = true)
    uint32 animationIndex = ~0u;

    HYP_FIELD(Property = "Status", Editor = true)
    AnimationPlaybackStatus status = AnimationPlaybackStatus::STOPPED;

    HYP_FIELD(Property = "LoopMode", Editor = true)
    AnimationLoopMode loopMode = AnimationLoopMode::ONCE;

    HYP_FIELD(Property = "Speed", Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Editor = true)
    float currentTime = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(animationIndex);
        hc.Add(status);
        hc.Add(loopMode);
        hc.Add(speed);
        hc.Add(currentTime);

        return hc;
    }
};

HYP_STRUCT(Component)
struct AnimationComponent
{
    HYP_STRUCT_BODY(AnimationComponent);

    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AnimationPlaybackState playbackState;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(playbackState);

        return hc;
    }
};

} // namespace Hyperion
