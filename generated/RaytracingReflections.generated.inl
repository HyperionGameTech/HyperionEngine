#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region RaytracingReflectionsConfig Reflection Data

HYP_BEGIN_STRUCT(RaytracingReflectionsConfig, 329, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Rendering.RayTracing"))
    HypField(NAME(HYP_STR(Extent)), &RaytracingReflectionsConfig::extent, offsetof(RaytracingReflectionsConfig, extent), Span<const HypClassAttribute> { {HypClassAttribute("jsonignore", true) } }),
    HypField(NAME(HYP_STR(PathTracing)), &RaytracingReflectionsConfig::pathTracing, offsetof(RaytracingReflectionsConfig, pathTracing), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "PathTracing.Enabled") } })
HYP_END_STRUCT

#pragma endregion RaytracingReflectionsConfig Reflection Data

} // namespace hyperion

