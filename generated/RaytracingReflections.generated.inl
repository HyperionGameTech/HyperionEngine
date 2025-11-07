#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region RaytracingReflectionsConfig Reflection Data

HYP_BEGIN_STRUCT(RaytracingReflectionsConfig, 329, 0, {}, ClassAttribute("configname", "GlobalConfig"),ClassAttribute("jsonpath", "Rendering.RayTracing"))
    Field(NAME(HYP_STR(Extent)), &RaytracingReflectionsConfig::extent, offsetof(RaytracingReflectionsConfig, extent), Span<const ClassAttribute> { {ClassAttribute("jsonignore", true) } }),
    Field(NAME(HYP_STR(PathTracing)), &RaytracingReflectionsConfig::pathTracing, offsetof(RaytracingReflectionsConfig, pathTracing), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "PathTracing.Enabled") } })
HYP_END_STRUCT

#pragma endregion RaytracingReflectionsConfig Reflection Data

} // namespace hyperion

