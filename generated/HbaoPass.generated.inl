#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region HBAOConfig Reflection Data

HYP_BEGIN_STRUCT(HBAOConfig, 274, 0, {}, ClassAttribute("configname", "GlobalConfig"),ClassAttribute("jsonpath", "Rendering.HBAO"))
    Field(NAME(HYP_STR(Radius)), &HBAOConfig::radius, offsetof(HBAOConfig, radius)),
    Field(NAME(HYP_STR(Power)), &HBAOConfig::power, offsetof(HBAOConfig, power)),
    Field(NAME(HYP_STR(UseTemporalBlending)), &HBAOConfig::useTemporalBlending, offsetof(HBAOConfig, useTemporalBlending))
HYP_END_STRUCT

#pragma endregion HBAOConfig Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region HBAO Reflection Data

HYP_BEGIN_CLASS(HBAO, 69, 0, NAME("FullScreenPass"), ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion HBAO Reflection Data

} // namespace hyperion

