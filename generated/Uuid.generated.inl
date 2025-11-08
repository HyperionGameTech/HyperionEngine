#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Uuid Reflection Data

HYP_BEGIN_STRUCT(Uuid, 252, 0, {}, ClassAttribute("serialize", "bitwise"))
    Field(NAME(HYP_STR(Data0)), &Uuid::data0, offsetof(Uuid, data0), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Data0") } }),
    Field(NAME(HYP_STR(Data1)), &Uuid::data1, offsetof(Uuid, data1), Span<const ClassAttribute> { {ClassAttribute("serialize", true), ClassAttribute("property", "Data1") } }),
    Method(NAME(HYP_STR(ToString)), &Uuid::ToString)
HYP_END_STRUCT

#pragma endregion Uuid Reflection Data

} // namespace hyperion

