#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region WeakName Reflection Data

HYP_BEGIN_STRUCT(WeakName, 230, 0, {})
    HypMethod(NAME(HYP_STR(ToString)), &WeakName::ToString, Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion WeakName Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Name Reflection Data

HYP_BEGIN_STRUCT(Name, 231, 0, {})
    HypMethod(NAME(HYP_STR(ToString)), &Name::ToString, Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion Name Reflection Data

} // namespace hyperion

