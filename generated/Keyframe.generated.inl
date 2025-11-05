#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Keyframe Reflection Data

HYP_BEGIN_STRUCT(Keyframe, 388, 0, {})
    Field(NAME(HYP_STR(Time)), &Keyframe::time, offsetof(Keyframe, time), Span<const ClassAttribute> { {ClassAttribute("property", "Time"), ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Transform)), &Keyframe::transform, offsetof(Keyframe, transform), Span<const ClassAttribute> { {ClassAttribute("property", "Transform"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(Blend)), &Keyframe::Blend)
HYP_END_STRUCT

#pragma endregion Keyframe Reflection Data

} // namespace hyperion

