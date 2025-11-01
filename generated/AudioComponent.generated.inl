#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AudioLoopMode Reflection Data

HYP_BEGIN_ENUM(AudioLoopMode, 375, 0, {})
    HypConstant(NAME(HYP_STR(ALM_ONCE)), AudioLoopMode::ALM_ONCE),
    HypConstant(NAME(HYP_STR(ALM_REPEAT)), AudioLoopMode::ALM_REPEAT)
HYP_END_ENUM

#pragma endregion AudioLoopMode Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region AudioComponent Reflection Data

HYP_BEGIN_STRUCT(AudioComponent, 376, 0, {}, HypClassAttribute("component", true),HypClassAttribute("label", "Audio Component"),HypClassAttribute("description", "Controls the state of an audio source."),HypClassAttribute("editor", true))
    HypField(NAME(HYP_STR(AudioSource)), &AudioComponent::audioSource, offsetof(AudioComponent, audioSource), Span<const HypClassAttribute> { {HypClassAttribute("property", "AudioSource"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(PlaybackState)), &AudioComponent::playbackState, offsetof(AudioComponent, playbackState), Span<const HypClassAttribute> { {HypClassAttribute("property", "PlaybackState"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Flags)), &AudioComponent::flags, offsetof(AudioComponent, flags), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(LastPosition)), &AudioComponent::lastPosition, offsetof(AudioComponent, lastPosition), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } }),
    HypField(NAME(HYP_STR(Timer)), &AudioComponent::timer, offsetof(AudioComponent, timer), Span<const HypClassAttribute> { {HypClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion AudioComponent Reflection Data

HYP_REGISTER_COMPONENT(AudioComponent);
} // namespace hyperion


namespace hyperion {

#pragma region AudioComponentFlags Reflection Data

HYP_BEGIN_ENUM(AudioComponentFlags, 377, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), AudioComponentFlags::NONE),
    HypConstant(NAME(HYP_STR(INIT)), AudioComponentFlags::INIT)
HYP_END_ENUM

#pragma endregion AudioComponentFlags Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioPlaybackStatus Reflection Data

HYP_BEGIN_ENUM(AudioPlaybackStatus, 378, 0, {})
    HypConstant(NAME(HYP_STR(APS_STOPPED)), AudioPlaybackStatus::APS_STOPPED),
    HypConstant(NAME(HYP_STR(APS_PAUSED)), AudioPlaybackStatus::APS_PAUSED),
    HypConstant(NAME(HYP_STR(APS_PLAYING)), AudioPlaybackStatus::APS_PLAYING)
HYP_END_ENUM

#pragma endregion AudioPlaybackStatus Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioPlaybackState Reflection Data

HYP_BEGIN_STRUCT(AudioPlaybackState, 379, 0, {})
    HypField(NAME(HYP_STR(Status)), &AudioPlaybackState::status, offsetof(AudioPlaybackState, status), Span<const HypClassAttribute> { {HypClassAttribute("property", "Status"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(LoopMode)), &AudioPlaybackState::loopMode, offsetof(AudioPlaybackState, loopMode), Span<const HypClassAttribute> { {HypClassAttribute("property", "LoopMode"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Speed)), &AudioPlaybackState::speed, offsetof(AudioPlaybackState, speed), Span<const HypClassAttribute> { {HypClassAttribute("property", "Speed"), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(CurrentTime)), &AudioPlaybackState::currentTime, offsetof(AudioPlaybackState, currentTime), Span<const HypClassAttribute> { {HypClassAttribute("property", "CurrentTime"), HypClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion AudioPlaybackState Reflection Data

} // namespace hyperion

