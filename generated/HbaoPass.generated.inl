#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region HBAOConfig Reflection Data

HYP_BEGIN_STRUCT(HBAOConfig, 332, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Rendering.HBAO"))
    HypField(NAME(HYP_STR(Radius)), &HBAOConfig::radius, offsetof(HBAOConfig, radius)),
    HypField(NAME(HYP_STR(Power)), &HBAOConfig::power, offsetof(HBAOConfig, power)),
    HypField(NAME(HYP_STR(UseTemporalBlending)), &HBAOConfig::useTemporalBlending, offsetof(HBAOConfig, useTemporalBlending))
HYP_END_STRUCT

#pragma endregion HBAOConfig Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region HBAO Reflection Data

HYP_BEGIN_CLASS(HBAO, 128, 0, NAME("FullScreenPass"), HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion HBAO Reflection Data

} // namespace hyperion

