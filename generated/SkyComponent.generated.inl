#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region SkyComponent Reflection Data

HYP_BEGIN_STRUCT(SkyComponent, 403, 0, {}, HypClassAttribute("component", true),HypClassAttribute("label", "Sky Component"),HypClassAttribute("description", "Controls the rendering of a dynamic skydome."),HypClassAttribute("editor", true))
    HypField(NAME(HYP_STR(Subsystem)), &SkyComponent::subsystem, offsetof(SkyComponent, subsystem), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true) } })
HYP_END_STRUCT

#pragma endregion SkyComponent Reflection Data

HYP_REGISTER_COMPONENT(SkyComponent);
} // namespace hyperion

