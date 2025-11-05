#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region UITextbox Reflection Data

HYP_BEGIN_CLASS(UITextbox, 17, 0, NAME("UIPanel"))
    Method(NAME(HYP_STR(GetPlaceholder)), &UITextbox::GetPlaceholder, Span<const ClassAttribute> { {ClassAttribute("property", "Placeholder"), ClassAttribute("xmlattribute", "placeholder") } }),
    Method(NAME(HYP_STR(SetPlaceholder)), &UITextbox::SetPlaceholder, Span<const ClassAttribute> { {ClassAttribute("property", "Placeholder"), ClassAttribute("xmlattribute", "placeholder") } }),
    Method(NAME(HYP_STR(GetPlaceholderTextColor)), &UITextbox::GetPlaceholderTextColor),
    Field(NAME(HYP_STR(ClearOnSubmit)), &UITextbox::clearOnSubmit, offsetof(UITextbox, clearOnSubmit))
HYP_END_CLASS

#pragma endregion UITextbox Reflection Data

} // namespace hyperion

