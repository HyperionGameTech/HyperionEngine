#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region UITextbox Reflection Data

HYP_BEGIN_CLASS(UITextbox, 210, 0, NAME("UIPanel"))
    HypMethod(NAME(HYP_STR(GetPlaceholder)), &UITextbox::GetPlaceholder, Span<const HypClassAttribute> { {HypClassAttribute("property", "Placeholder"), HypClassAttribute("xmlattribute", "placeholder") } }),
    HypMethod(NAME(HYP_STR(SetPlaceholder)), &UITextbox::SetPlaceholder, Span<const HypClassAttribute> { {HypClassAttribute("property", "Placeholder"), HypClassAttribute("xmlattribute", "placeholder") } }),
    HypMethod(NAME(HYP_STR(GetPlaceholderTextColor)), &UITextbox::GetPlaceholderTextColor),
    HypField(NAME(HYP_STR(ClearOnSubmit)), &UITextbox::clearOnSubmit, offsetof(UITextbox, clearOnSubmit))
HYP_END_CLASS

#pragma endregion UITextbox Reflection Data

} // namespace hyperion

