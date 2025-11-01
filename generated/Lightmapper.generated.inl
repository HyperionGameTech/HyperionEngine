#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region LightmapperBase Reflection Data

HYP_BEGIN_CLASS(LightmapperBase, 120, 0, NAME("HypObjectBase"), HypClassAttribute("abstract", true))
HYP_END_CLASS

#pragma endregion LightmapperBase Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LightmapShadingType Reflection Data

HYP_BEGIN_ENUM(LightmapShadingType, 326, 0, {})
    HypConstant(NAME(HYP_STR(IRRADIANCE)), LightmapShadingType::IRRADIANCE),
    HypConstant(NAME(HYP_STR(RADIANCE)), LightmapShadingType::RADIANCE),
    HypConstant(NAME(HYP_STR(MAX)), LightmapShadingType::MAX)
HYP_END_ENUM

#pragma endregion LightmapShadingType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LightmapperConfig Reflection Data

HYP_BEGIN_STRUCT(LightmapperConfig, 327, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Lightmapper"))
    HypField(NAME(HYP_STR(TraceMode)), &LightmapperConfig::traceMode, offsetof(LightmapperConfig, traceMode)),
    HypField(NAME(HYP_STR(Radiance)), &LightmapperConfig::radiance, offsetof(LightmapperConfig, radiance)),
    HypField(NAME(HYP_STR(Irradiance)), &LightmapperConfig::irradiance, offsetof(LightmapperConfig, irradiance)),
    HypField(NAME(HYP_STR(NumSamples)), &LightmapperConfig::numSamples, offsetof(LightmapperConfig, numSamples)),
    HypField(NAME(HYP_STR(MaxRaysPerFrame)), &LightmapperConfig::maxRaysPerFrame, offsetof(LightmapperConfig, maxRaysPerFrame)),
    HypField(NAME(HYP_STR(IdealTrianglesPerJob)), &LightmapperConfig::idealTrianglesPerJob, offsetof(LightmapperConfig, idealTrianglesPerJob))
HYP_END_STRUCT

#pragma endregion LightmapperConfig Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LightmapTraceMode Reflection Data

HYP_BEGIN_ENUM(LightmapTraceMode, 328, 0, {})
    HypConstant(NAME(HYP_STR(GPU_PATH_TRACING)), LightmapTraceMode::GPU_PATH_TRACING),
    HypConstant(NAME(HYP_STR(CPU_PATH_TRACING)), LightmapTraceMode::CPU_PATH_TRACING),
    HypConstant(NAME(HYP_STR(MAX)), LightmapTraceMode::MAX)
HYP_END_ENUM

#pragma endregion LightmapTraceMode Reflection Data

} // namespace hyperion

