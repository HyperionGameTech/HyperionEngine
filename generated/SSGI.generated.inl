#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region SSGIConfig Reflection Data

HYP_BEGIN_STRUCT(SSGIConfig, 326, 0, {}, ClassAttribute("configname", "GlobalConfig"),ClassAttribute("jsonpath", "Rendering.SSGI"))
    Field(NAME(HYP_STR(Quality)), &SSGIConfig::quality, offsetof(SSGIConfig, quality), Span<const ClassAttribute> { {ClassAttribute("description", "The quality level of the SSGI effect. 0 = quarter res, 1 = half res") } }),
    Field(NAME(HYP_STR(Extent)), &SSGIConfig::extent, offsetof(SSGIConfig, extent), Span<const ClassAttribute> { {ClassAttribute("jsonignore", true) } })
HYP_END_STRUCT

#pragma endregion SSGIConfig Reflection Data

} // namespace hyperion

