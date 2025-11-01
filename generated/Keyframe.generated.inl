#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Keyframe Reflection Data

HYP_BEGIN_STRUCT(Keyframe, 367, 0, {})
    HypField(NAME(HYP_STR(Time)), &Keyframe::time, offsetof(Keyframe, time), Span<const HypClassAttribute> { {HypClassAttribute("property", "Time"), HypClassAttribute("serialize", true) } }),
    HypField(NAME(HYP_STR(Transform)), &Keyframe::transform, offsetof(Keyframe, transform), Span<const HypClassAttribute> { {HypClassAttribute("property", "Transform"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(Blend)), &Keyframe::Blend)
HYP_END_STRUCT

#pragma endregion Keyframe Reflection Data

} // namespace hyperion

