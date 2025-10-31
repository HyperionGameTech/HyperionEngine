#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region SSGIConfig Reflection Data

HYP_BEGIN_STRUCT(SSGIConfig, 319, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Rendering.SSGI"))
    HypField(NAME(HYP_STR(Quality)), &SSGIConfig::quality, offsetof(SSGIConfig, quality), Span<const HypClassAttribute> { {HypClassAttribute("description", "The quality level of the SSGI effect. 0 = quarter res, 1 = half res") } }),
    HypField(NAME(HYP_STR(Extent)), &SSGIConfig::extent, offsetof(SSGIConfig, extent), Span<const HypClassAttribute> { {HypClassAttribute("jsonignore", true) } })
HYP_END_STRUCT

#pragma endregion SSGIConfig Reflection Data

} // namespace hyperion

