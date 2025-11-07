#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region AnimationComponent Reflection Data

HYP_BEGIN_STRUCT(AnimationComponent, 372, 0, {}, ClassAttribute("component", true))
    Field(NAME(HYP_STR(PlaybackState)), &AnimationComponent::playbackState, offsetof(AnimationComponent, playbackState), Span<const ClassAttribute> { {ClassAttribute("property", "PlaybackState"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion AnimationComponent Reflection Data

HYP_REGISTER_COMPONENT(AnimationComponent);
} // namespace hyperion


namespace hyperion {

#pragma region AnimationPlaybackState Reflection Data

HYP_BEGIN_STRUCT(AnimationPlaybackState, 373, 0, {})
    Field(NAME(HYP_STR(AnimationIndex)), &AnimationPlaybackState::animationIndex, offsetof(AnimationPlaybackState, animationIndex), Span<const ClassAttribute> { {ClassAttribute("property", "AnimationIndex"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Status)), &AnimationPlaybackState::status, offsetof(AnimationPlaybackState, status), Span<const ClassAttribute> { {ClassAttribute("property", "Status"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(LoopMode)), &AnimationPlaybackState::loopMode, offsetof(AnimationPlaybackState, loopMode), Span<const ClassAttribute> { {ClassAttribute("property", "LoopMode"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Speed)), &AnimationPlaybackState::speed, offsetof(AnimationPlaybackState, speed), Span<const ClassAttribute> { {ClassAttribute("property", "Speed"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(CurrentTime)), &AnimationPlaybackState::currentTime, offsetof(AnimationPlaybackState, currentTime), Span<const ClassAttribute> { {ClassAttribute("property", "CurrentTime"), ClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion AnimationPlaybackState Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AnimationLoopMode Reflection Data

HYP_BEGIN_ENUM(AnimationLoopMode, 374, 0, {})
    StaticField(NAME(HYP_STR(ONCE)), AnimationLoopMode::ONCE),
    StaticField(NAME(HYP_STR(REPEAT)), AnimationLoopMode::REPEAT)
HYP_END_ENUM

#pragma endregion AnimationLoopMode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AnimationPlaybackStatus Reflection Data

HYP_BEGIN_ENUM(AnimationPlaybackStatus, 375, 0, {})
    StaticField(NAME(HYP_STR(STOPPED)), AnimationPlaybackStatus::STOPPED),
    StaticField(NAME(HYP_STR(PAUSED)), AnimationPlaybackStatus::PAUSED),
    StaticField(NAME(HYP_STR(PLAYING)), AnimationPlaybackStatus::PLAYING)
HYP_END_ENUM

#pragma endregion AnimationPlaybackStatus Reflection Data

} // namespace hyperion

