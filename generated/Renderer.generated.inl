#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region PassData Reflection Data

HYP_BEGIN_CLASS(PassData, 92, 6, NAME("HypObjectBase"), HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion PassData Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RendererConfig Reflection Data

HYP_BEGIN_STRUCT(RendererConfig, 292, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Rendering"))
    HypField(NAME(HYP_STR(PathTracer)), &RendererConfig::pathTracer, offsetof(RendererConfig, pathTracer), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "RayTracing.PathTracing.Enabled") } }),
    HypField(NAME(HYP_STR(RaytracingReflections)), &RendererConfig::raytracingReflections, offsetof(RendererConfig, raytracingReflections), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "RayTracing.Reflections.Enabled") } }),
    HypField(NAME(HYP_STR(RaytracingGlobalIllumination)), &RendererConfig::raytracingGlobalIllumination, offsetof(RendererConfig, raytracingGlobalIllumination), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "RayTracing.GI.Enabled") } }),
    HypField(NAME(HYP_STR(HbaoEnabled)), &RendererConfig::hbaoEnabled, offsetof(RendererConfig, hbaoEnabled), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "HBAO.Enabled") } }),
    HypField(NAME(HYP_STR(HbilEnabled)), &RendererConfig::hbilEnabled, offsetof(RendererConfig, hbilEnabled), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "HBIL.Enabled") } }),
    HypField(NAME(HYP_STR(SsgiEnabled)), &RendererConfig::ssgiEnabled, offsetof(RendererConfig, ssgiEnabled), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "SSGI.Enabled") } }),
    HypField(NAME(HYP_STR(EnvGridGiEnabled)), &RendererConfig::envGridGiEnabled, offsetof(RendererConfig, envGridGiEnabled), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "EnvGrid.GI.Enabled") } }),
    HypField(NAME(HYP_STR(EnvGridRadianceEnabled)), &RendererConfig::envGridRadianceEnabled, offsetof(RendererConfig, envGridRadianceEnabled), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "EnvGrid.Reflections.Enabled") } }),
    HypField(NAME(HYP_STR(TaaEnabled)), &RendererConfig::taaEnabled, offsetof(RendererConfig, taaEnabled), Span<const HypClassAttribute> { {HypClassAttribute("jsonpath", "TAA.Enabled") } })
HYP_END_STRUCT

#pragma endregion RendererConfig Reflection Data

} // namespace hyperion

