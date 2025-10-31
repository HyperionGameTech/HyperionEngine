#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region AnimationTrack Reflection Data

HYP_BEGIN_CLASS(AnimationTrack, 190, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetBoneName)), &AnimationTrack::GetBoneName),
    HypMethod(NAME(HYP_STR(SetBoneName)), &AnimationTrack::SetBoneName),
    HypMethod(NAME(HYP_STR(AddKeyframe)), &AnimationTrack::AddKeyframe),
    HypMethod(NAME(HYP_STR(GetKeyframes)), &AnimationTrack::GetKeyframes),
    HypMethod(NAME(HYP_STR(GetLength)), &AnimationTrack::GetLength),
    HypMethod(NAME(HYP_STR(GetKeyframe)), &AnimationTrack::GetKeyframe),
    HypField(NAME(HYP_STR(BoneName)), &AnimationTrack::m_boneName, offsetof(AnimationTrack, m_boneName)),
    HypField(NAME(HYP_STR(Keyframes)), &AnimationTrack::m_keyframes, offsetof(AnimationTrack, m_keyframes))
HYP_END_CLASS

#pragma endregion AnimationTrack Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Animation Reflection Data

HYP_BEGIN_CLASS(Animation, 191, 0, NAME("HypObjectBase"))
    HypMethod(NAME(HYP_STR(GetName)), &Animation::GetName, Span<const HypClassAttribute> { {HypClassAttribute("property", "Name") } }),
    HypMethod(NAME(HYP_STR(SetName)), &Animation::SetName, Span<const HypClassAttribute> { {HypClassAttribute("property", "Name") } }),
    HypMethod(NAME(HYP_STR(GetLength)), &Animation::GetLength, Span<const HypClassAttribute> { {HypClassAttribute("property", "Length"), HypClassAttribute("transient", true) } }),
    HypMethod(NAME(HYP_STR(AddTrack)), &Animation::AddTrack),
    HypMethod(NAME(HYP_STR(GetTracks)), &Animation::GetTracks, Span<const HypClassAttribute> { {HypClassAttribute("property", "Tracks") } }),
    HypMethod(NAME(HYP_STR(SetTracks)), &Animation::SetTracks, Span<const HypClassAttribute> { {HypClassAttribute("property", "Tracks") } }),
    HypMethod(NAME(HYP_STR(GetTrack)), &Animation::GetTrack),
    HypMethod(NAME(HYP_STR(NumTracks)), &Animation::NumTracks),
    HypMethod(NAME(HYP_STR(Apply)), &Animation::Apply),
    HypMethod(NAME(HYP_STR(ApplyBlended)), &Animation::ApplyBlended),
    HypField(NAME(HYP_STR(Name)), &Animation::m_name, offsetof(Animation, m_name), Span<const HypClassAttribute> { {HypClassAttribute("property", "Name") } }),
    HypField(NAME(HYP_STR(Tracks)), &Animation::m_tracks, offsetof(Animation, m_tracks), Span<const HypClassAttribute> { {HypClassAttribute("property", "Tracks") } })
HYP_END_CLASS

#pragma endregion Animation Reflection Data

} // namespace hyperion

