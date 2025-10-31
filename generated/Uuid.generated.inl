#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Uuid Reflection Data

HYP_BEGIN_STRUCT(Uuid, 250, 0, {}, HypClassAttribute("serialize", "bitwise"))
    HypField(NAME(HYP_STR(Data0)), &Uuid::data0, offsetof(Uuid, data0), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Data0") } }),
    HypField(NAME(HYP_STR(Data1)), &Uuid::data1, offsetof(Uuid, data1), Span<const HypClassAttribute> { {HypClassAttribute("serialize", true), HypClassAttribute("property", "Data1") } }),
    HypMethod(NAME(HYP_STR(ToString)), &Uuid::ToString)
HYP_END_STRUCT

#pragma endregion Uuid Reflection Data

} // namespace hyperion

