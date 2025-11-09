#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AudioLoopMode Reflection Data

HYP_BEGIN_ENUM(AudioLoopMode, 401, 0, {})
    StaticField(NAME(HYP_STR(ALM_ONCE)), AudioLoopMode::ALM_ONCE),
    StaticField(NAME(HYP_STR(ALM_REPEAT)), AudioLoopMode::ALM_REPEAT)
HYP_END_ENUM

#pragma endregion AudioLoopMode Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region AudioComponent Reflection Data

HYP_BEGIN_STRUCT(AudioComponent, 402, 0, {}, ClassAttribute("component", true),ClassAttribute("label", "Audio Component"),ClassAttribute("description", "Controls the state of an audio source."),ClassAttribute("editor", true))
    Field(NAME(HYP_STR(AudioSource)), &AudioComponent::audioSource, offsetof(AudioComponent, audioSource), Span<const ClassAttribute> { {ClassAttribute("property", "AudioSource"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(PlaybackState)), &AudioComponent::playbackState, offsetof(AudioComponent, playbackState), Span<const ClassAttribute> { {ClassAttribute("property", "PlaybackState"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Flags)), &AudioComponent::flags, offsetof(AudioComponent, flags), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(LastPosition)), &AudioComponent::lastPosition, offsetof(AudioComponent, lastPosition), Span<const ClassAttribute> { {ClassAttribute("transient", true) } }),
    Field(NAME(HYP_STR(Timer)), &AudioComponent::timer, offsetof(AudioComponent, timer), Span<const ClassAttribute> { {ClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion AudioComponent Reflection Data

HYP_REGISTER_COMPONENT(AudioComponent);
} // namespace hyperion


namespace hyperion {

#pragma region AudioComponentFlags Reflection Data

HYP_BEGIN_ENUM(AudioComponentFlags, 403, 0, {})
    StaticField(NAME(HYP_STR(NONE)), AudioComponentFlags::NONE),
    StaticField(NAME(HYP_STR(INIT)), AudioComponentFlags::INIT)
HYP_END_ENUM

#pragma endregion AudioComponentFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioPlaybackStatus Reflection Data

HYP_BEGIN_ENUM(AudioPlaybackStatus, 404, 0, {})
    StaticField(NAME(HYP_STR(APS_STOPPED)), AudioPlaybackStatus::APS_STOPPED),
    StaticField(NAME(HYP_STR(APS_PAUSED)), AudioPlaybackStatus::APS_PAUSED),
    StaticField(NAME(HYP_STR(APS_PLAYING)), AudioPlaybackStatus::APS_PLAYING)
HYP_END_ENUM

#pragma endregion AudioPlaybackStatus Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioPlaybackState Reflection Data

HYP_BEGIN_STRUCT(AudioPlaybackState, 405, 0, {})
    Field(NAME(HYP_STR(Status)), &AudioPlaybackState::status, offsetof(AudioPlaybackState, status), Span<const ClassAttribute> { {ClassAttribute("property", "Status"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(LoopMode)), &AudioPlaybackState::loopMode, offsetof(AudioPlaybackState, loopMode), Span<const ClassAttribute> { {ClassAttribute("property", "LoopMode"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Speed)), &AudioPlaybackState::speed, offsetof(AudioPlaybackState, speed), Span<const ClassAttribute> { {ClassAttribute("property", "Speed"), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(CurrentTime)), &AudioPlaybackState::currentTime, offsetof(AudioPlaybackState, currentTime), Span<const ClassAttribute> { {ClassAttribute("property", "CurrentTime"), ClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion AudioPlaybackState Reflection Data

} // namespace hyperion

