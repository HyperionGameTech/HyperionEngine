#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region AnimationComponent Reflection Data

HYP_BEGIN_STRUCT(AnimationComponent, 371, 0, {}, HypClassAttribute("component", true))
    HypField(NAME(HYP_STR(PlaybackState)), &AnimationComponent::playbackState, offsetof(AnimationComponent, playbackState), Span<const HypClassAttribute> { {HypClassAttribute("property", "PlaybackState"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion AnimationComponent Reflection Data

HYP_REGISTER_COMPONENT(AnimationComponent);
} // namespace hyperion


namespace hyperion {

#pragma region AnimationPlaybackState Reflection Data

HYP_BEGIN_STRUCT(AnimationPlaybackState, 372, 0, {})
    HypField(NAME(HYP_STR(AnimationIndex)), &AnimationPlaybackState::animationIndex, offsetof(AnimationPlaybackState, animationIndex), Span<const HypClassAttribute> { {HypClassAttribute("property", "AnimationIndex"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Status)), &AnimationPlaybackState::status, offsetof(AnimationPlaybackState, status), Span<const HypClassAttribute> { {HypClassAttribute("property", "Status"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(LoopMode)), &AnimationPlaybackState::loopMode, offsetof(AnimationPlaybackState, loopMode), Span<const HypClassAttribute> { {HypClassAttribute("property", "LoopMode"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Speed)), &AnimationPlaybackState::speed, offsetof(AnimationPlaybackState, speed), Span<const HypClassAttribute> { {HypClassAttribute("property", "Speed"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(CurrentTime)), &AnimationPlaybackState::currentTime, offsetof(AnimationPlaybackState, currentTime), Span<const HypClassAttribute> { {HypClassAttribute("property", "CurrentTime"), HypClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion AnimationPlaybackState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AnimationLoopMode Reflection Data

HYP_BEGIN_ENUM(AnimationLoopMode, 373, 0, {})
    HypConstant(NAME(HYP_STR(ONCE)), AnimationLoopMode::ONCE),
    HypConstant(NAME(HYP_STR(REPEAT)), AnimationLoopMode::REPEAT)
HYP_END_ENUM

#pragma endregion AnimationLoopMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AnimationPlaybackStatus Reflection Data

HYP_BEGIN_ENUM(AnimationPlaybackStatus, 374, 0, {})
    HypConstant(NAME(HYP_STR(STOPPED)), AnimationPlaybackStatus::STOPPED),
    HypConstant(NAME(HYP_STR(PAUSED)), AnimationPlaybackStatus::PAUSED),
    HypConstant(NAME(HYP_STR(PLAYING)), AnimationPlaybackStatus::PLAYING)
HYP_END_ENUM

#pragma endregion AnimationPlaybackStatus Reflection Data

} // namespace hyperion

