#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Color Reflection Data

HYP_BEGIN_STRUCT(Color, 239, 0, {})
    HypMethod(NAME(HYP_STR(GetRed)), &Color::GetRed, Span<const HypClassAttribute> { {HypClassAttribute("property", "Red"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetRed)), &Color::SetRed, Span<const HypClassAttribute> { {HypClassAttribute("property", "Red"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(GetGreen)), &Color::GetGreen, Span<const HypClassAttribute> { {HypClassAttribute("property", "Green"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetGreen)), &Color::SetGreen, Span<const HypClassAttribute> { {HypClassAttribute("property", "Green"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(GetBlue)), &Color::GetBlue, Span<const HypClassAttribute> { {HypClassAttribute("property", "Blue"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetBlue)), &Color::SetBlue, Span<const HypClassAttribute> { {HypClassAttribute("property", "Blue"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(GetAlpha)), &Color::GetAlpha, Span<const HypClassAttribute> { {HypClassAttribute("property", "Alpha"), HypClassAttribute("serialize", true) } }),
    HypMethod(NAME(HYP_STR(SetAlpha)), &Color::SetAlpha, Span<const HypClassAttribute> { {HypClassAttribute("property", "Alpha"), HypClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion Color Reflection Data

} // namespace hyperion

