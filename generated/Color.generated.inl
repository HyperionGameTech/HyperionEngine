#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Color Reflection Data

HYP_BEGIN_STRUCT(Color, 241, 0, {})
    Method(NAME(HYP_STR(GetRed)), &Color::GetRed, Span<const ClassAttribute> { {ClassAttribute("property", "Red"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetRed)), &Color::SetRed, Span<const ClassAttribute> { {ClassAttribute("property", "Red"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(GetGreen)), &Color::GetGreen, Span<const ClassAttribute> { {ClassAttribute("property", "Green"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetGreen)), &Color::SetGreen, Span<const ClassAttribute> { {ClassAttribute("property", "Green"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(GetBlue)), &Color::GetBlue, Span<const ClassAttribute> { {ClassAttribute("property", "Blue"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetBlue)), &Color::SetBlue, Span<const ClassAttribute> { {ClassAttribute("property", "Blue"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(GetAlpha)), &Color::GetAlpha, Span<const ClassAttribute> { {ClassAttribute("property", "Alpha"), ClassAttribute("serialize", true) } }),
    Method(NAME(HYP_STR(SetAlpha)), &Color::SetAlpha, Span<const ClassAttribute> { {ClassAttribute("property", "Alpha"), ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion Color Reflection Data

} // namespace hyperion

