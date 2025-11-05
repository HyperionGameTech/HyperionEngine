#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region SkyComponent Reflection Data

HYP_BEGIN_STRUCT(SkyComponent, 404, 0, {}, ClassAttribute("component", true),ClassAttribute("label", "Sky Component"),ClassAttribute("description", "Controls the rendering of a dynamic skydome."),ClassAttribute("editor", true))
    Field(NAME(HYP_STR(Subsystem)), &SkyComponent::subsystem, offsetof(SkyComponent, subsystem), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion SkyComponent Reflection Data

HYP_REGISTER_COMPONENT(SkyComponent);
} // namespace hyperion

