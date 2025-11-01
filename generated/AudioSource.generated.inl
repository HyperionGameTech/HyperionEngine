#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AudioSource Reflection Data

HYP_BEGIN_CLASS(AudioSource, 30, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetFormat)), &AudioSource::GetFormat, Span<const HypClassAttribute> { {HypClassAttribute("property", "Format"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetFormat)), &AudioSource::SetFormat, Span<const HypClassAttribute> { {HypClassAttribute("property", "Format"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetFreq)), &AudioSource::GetFreq, Span<const HypClassAttribute> { {HypClassAttribute("property", "Freq"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetFreq)), &AudioSource::SetFreq, Span<const HypClassAttribute> { {HypClassAttribute("property", "Freq"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetData)), &AudioSource::GetData, Span<const HypClassAttribute> { {HypClassAttribute("property", "Data"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetData)), &AudioSource::SetData, Span<const HypClassAttribute> { {HypClassAttribute("property", "Data"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetSampleLength)), &AudioSource::GetSampleLength, Span<const HypClassAttribute> { {HypClassAttribute("property", "SampleLength"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetSampleLength)), &AudioSource::SetSampleLength, Span<const HypClassAttribute> { {HypClassAttribute("property", "SampleLength"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(GetDuration)), &AudioSource::GetDuration, Span<const HypClassAttribute> { {HypClassAttribute("property", "Duration"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypMethod(NAME(HYP_STR(SetDuration)), &AudioSource::SetDuration, Span<const HypClassAttribute> { {HypClassAttribute("property", "Duration"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } })
HYP_END_CLASS

#pragma endregion AudioSource Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioSourceFormat Reflection Data

HYP_BEGIN_ENUM(AudioSourceFormat, 227, 0, {})
    HypConstant(NAME(HYP_STR(MONO8)), AudioSourceFormat::MONO8),
    HypConstant(NAME(HYP_STR(MONO16)), AudioSourceFormat::MONO16),
    HypConstant(NAME(HYP_STR(STEREO8)), AudioSourceFormat::STEREO8),
    HypConstant(NAME(HYP_STR(STEREO16)), AudioSourceFormat::STEREO16)
HYP_END_ENUM

#pragma endregion AudioSourceFormat Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region AudioSourceState Reflection Data

HYP_BEGIN_ENUM(AudioSourceState, 228, 0, {})
    HypConstant(NAME(HYP_STR(UNDEFINED)), AudioSourceState::UNDEFINED),
    HypConstant(NAME(HYP_STR(STOPPED)), AudioSourceState::STOPPED),
    HypConstant(NAME(HYP_STR(PLAYING)), AudioSourceState::PLAYING),
    HypConstant(NAME(HYP_STR(PAUSED)), AudioSourceState::PAUSED)
HYP_END_ENUM

#pragma endregion AudioSourceState Reflection Data

} // namespace hyperion

