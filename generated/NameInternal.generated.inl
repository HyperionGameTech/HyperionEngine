#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Name Reflection Data

HYP_BEGIN_STRUCT(Name, 232, 0, {})
    Method(NAME(HYP_STR(ToString)), &Name::ToString, Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion Name Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region StringHash Reflection Data

HYP_BEGIN_STRUCT(StringHash, 233, 0, {})
    Method(NAME(HYP_STR(ToString)), &StringHash::ToString, Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion StringHash Reflection Data

} // namespace hyperion

