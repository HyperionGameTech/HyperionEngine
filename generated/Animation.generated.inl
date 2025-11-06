#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region AnimationTrack Reflection Data

HYP_BEGIN_CLASS(AnimationTrack, 191, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetBoneName)), &AnimationTrack::GetBoneName),
    Method(NAME(HYP_STR(SetBoneName)), &AnimationTrack::SetBoneName),
    Method(NAME(HYP_STR(AddKeyframe)), &AnimationTrack::AddKeyframe),
    Method(NAME(HYP_STR(GetKeyframes)), &AnimationTrack::GetKeyframes),
    Method(NAME(HYP_STR(GetLength)), &AnimationTrack::GetLength),
    Method(NAME(HYP_STR(GetKeyframe)), &AnimationTrack::GetKeyframe),
    Field(NAME(HYP_STR(BoneName)), &AnimationTrack::m_boneName, offsetof(AnimationTrack, m_boneName)),
    Field(NAME(HYP_STR(Keyframes)), &AnimationTrack::m_keyframes, offsetof(AnimationTrack, m_keyframes))
HYP_END_CLASS

#pragma endregion AnimationTrack Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Animation Reflection Data

HYP_BEGIN_CLASS(Animation, 192, 0, NAME("ObjectBase"))
    Method(NAME(HYP_STR(GetName)), &Animation::GetName, Span<const ClassAttribute> { {ClassAttribute("property", "Name") } }),
    Method(NAME(HYP_STR(SetName)), &Animation::SetName, Span<const ClassAttribute> { {ClassAttribute("property", "Name") } }),
    Method(NAME(HYP_STR(GetLength)), &Animation::GetLength, Span<const ClassAttribute> { {ClassAttribute("property", "Length"), ClassAttribute("transient", true) } }),
    Method(NAME(HYP_STR(AddTrack)), &Animation::AddTrack),
    Method(NAME(HYP_STR(GetTracks)), &Animation::GetTracks, Span<const ClassAttribute> { {ClassAttribute("property", "Tracks") } }),
    Method(NAME(HYP_STR(SetTracks)), &Animation::SetTracks, Span<const ClassAttribute> { {ClassAttribute("property", "Tracks") } }),
    Method(NAME(HYP_STR(GetTrack)), &Animation::GetTrack),
    Method(NAME(HYP_STR(NumTracks)), &Animation::NumTracks),
    Method(NAME(HYP_STR(Apply)), &Animation::Apply),
    Method(NAME(HYP_STR(ApplyBlended)), &Animation::ApplyBlended),
    Field(NAME(HYP_STR(Name)), &Animation::m_name, offsetof(Animation, m_name), Span<const ClassAttribute> { {ClassAttribute("property", "Name") } }),
    Field(NAME(HYP_STR(Tracks)), &Animation::m_tracks, offsetof(Animation, m_tracks), Span<const ClassAttribute> { {ClassAttribute("property", "Tracks") } })
HYP_END_CLASS

#pragma endregion Animation Reflection Data

} // namespace hyperion

