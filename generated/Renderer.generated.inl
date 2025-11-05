#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region PassData Reflection Data

HYP_BEGIN_CLASS(PassData, 88, 6, NAME("HypObjectBase"), ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion PassData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RendererConfig Reflection Data

HYP_BEGIN_STRUCT(RendererConfig, 324, 0, {}, ClassAttribute("configname", "GlobalConfig"),ClassAttribute("jsonpath", "Rendering"))
    Field(NAME(HYP_STR(PathTracer)), &RendererConfig::pathTracer, offsetof(RendererConfig, pathTracer), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "RayTracing.PathTracing.Enabled") } }),
    Field(NAME(HYP_STR(RaytracingReflections)), &RendererConfig::raytracingReflections, offsetof(RendererConfig, raytracingReflections), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "RayTracing.Reflections.Enabled") } }),
    Field(NAME(HYP_STR(RaytracingGlobalIllumination)), &RendererConfig::raytracingGlobalIllumination, offsetof(RendererConfig, raytracingGlobalIllumination), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "RayTracing.GI.Enabled") } }),
    Field(NAME(HYP_STR(HbaoEnabled)), &RendererConfig::hbaoEnabled, offsetof(RendererConfig, hbaoEnabled), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "HBAO.Enabled") } }),
    Field(NAME(HYP_STR(HbilEnabled)), &RendererConfig::hbilEnabled, offsetof(RendererConfig, hbilEnabled), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "HBIL.Enabled") } }),
    Field(NAME(HYP_STR(SsgiEnabled)), &RendererConfig::ssgiEnabled, offsetof(RendererConfig, ssgiEnabled), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "SSGI.Enabled") } }),
    Field(NAME(HYP_STR(EnvGridGiEnabled)), &RendererConfig::envGridGiEnabled, offsetof(RendererConfig, envGridGiEnabled), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "EnvGrid.GI.Enabled") } }),
    Field(NAME(HYP_STR(EnvGridRadianceEnabled)), &RendererConfig::envGridRadianceEnabled, offsetof(RendererConfig, envGridRadianceEnabled), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "EnvGrid.Reflections.Enabled") } }),
    Field(NAME(HYP_STR(TaaEnabled)), &RendererConfig::taaEnabled, offsetof(RendererConfig, taaEnabled), Span<const ClassAttribute> { {ClassAttribute("jsonpath", "TAA.Enabled") } })
HYP_END_STRUCT

#pragma endregion RendererConfig Reflection Data

} // namespace hyperion

