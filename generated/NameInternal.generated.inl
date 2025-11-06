#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region WeakName Reflection Data

HYP_BEGIN_STRUCT(WeakName, 231, 0, {})
    Method(NAME(HYP_STR(ToString)), &WeakName::ToString, Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion WeakName Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region Name Reflection Data

HYP_BEGIN_STRUCT(Name, 232, 0, {})
    Method(NAME(HYP_STR(ToString)), &Name::ToString, Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion Name Reflection Data

} // namespace hyperion

