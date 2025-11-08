#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AudioSource Reflection Data

HYP_BEGIN_CLASS(AudioSource, 78, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetFormat)), &AudioSource::GetFormat, Span<const ClassAttribute> { {ClassAttribute("property", "Format"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetFormat)), &AudioSource::SetFormat, Span<const ClassAttribute> { {ClassAttribute("property", "Format"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetFreq)), &AudioSource::GetFreq, Span<const ClassAttribute> { {ClassAttribute("property", "Freq"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetFreq)), &AudioSource::SetFreq, Span<const ClassAttribute> { {ClassAttribute("property", "Freq"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetData)), &AudioSource::GetData, Span<const ClassAttribute> { {ClassAttribute("property", "Data"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetData)), &AudioSource::SetData, Span<const ClassAttribute> { {ClassAttribute("property", "Data"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetSampleLength)), &AudioSource::GetSampleLength, Span<const ClassAttribute> { {ClassAttribute("property", "SampleLength"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetSampleLength)), &AudioSource::SetSampleLength, Span<const ClassAttribute> { {ClassAttribute("property", "SampleLength"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(GetDuration)), &AudioSource::GetDuration, Span<const ClassAttribute> { {ClassAttribute("property", "Duration"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Method(NAME(HYP_STR(SetDuration)), &AudioSource::SetDuration, Span<const ClassAttribute> { {ClassAttribute("property", "Duration"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } })
HYP_END_CLASS

#pragma endregion AudioSource Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioSourceFormat Reflection Data

HYP_BEGIN_ENUM(AudioSourceFormat, 290, 0, {})
    StaticField(NAME(HYP_STR(MONO8)), AudioSourceFormat::MONO8),
    StaticField(NAME(HYP_STR(MONO16)), AudioSourceFormat::MONO16),
    StaticField(NAME(HYP_STR(STEREO8)), AudioSourceFormat::STEREO8),
    StaticField(NAME(HYP_STR(STEREO16)), AudioSourceFormat::STEREO16)
HYP_END_ENUM

#pragma endregion AudioSourceFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioSourceState Reflection Data

HYP_BEGIN_ENUM(AudioSourceState, 291, 0, {})
    StaticField(NAME(HYP_STR(UNDEFINED)), AudioSourceState::UNDEFINED),
    StaticField(NAME(HYP_STR(STOPPED)), AudioSourceState::STOPPED),
    StaticField(NAME(HYP_STR(PLAYING)), AudioSourceState::PLAYING),
    StaticField(NAME(HYP_STR(PAUSED)), AudioSourceState::PAUSED)
HYP_END_ENUM

#pragma endregion AudioSourceState Reflection Data

} // namespace hyperion

