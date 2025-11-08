#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region SSRRendererConfig Reflection Data

HYP_BEGIN_STRUCT(SSRRendererConfig, 339, 0, {}, ClassAttribute("configname", "GlobalConfig"),ClassAttribute("jsonpath", "Rendering.SSR"))
    Field(NAME(HYP_STR(Enabled)), &SSRRendererConfig::enabled, offsetof(SSRRendererConfig, enabled)),
    Field(NAME(HYP_STR(Quality)), &SSRRendererConfig::quality, offsetof(SSRRendererConfig, quality), Span<const ClassAttribute> { {ClassAttribute("description", "The quality level of the SSR effect. 0 = low, 1 = medium, 2 = high") } }),
    Field(NAME(HYP_STR(RoughnessScattering)), &SSRRendererConfig::roughnessScattering, offsetof(SSRRendererConfig, roughnessScattering), Span<const ClassAttribute> { {ClassAttribute("description", "Enables scattering of rays based on the roughness of the surface. May cause artifacts due to temporal instability.") } }),
    Field(NAME(HYP_STR(ConeTracing)), &SSRRendererConfig::coneTracing, offsetof(SSRRendererConfig, coneTracing), Span<const ClassAttribute> { {ClassAttribute("description", "Enables cone tracing for the SSR effect. Causes the result to become blurrier based on distance of the reflection.") } }),
    Field(NAME(HYP_STR(RayStep)), &SSRRendererConfig::rayStep, offsetof(SSRRendererConfig, rayStep), Span<const ClassAttribute> { {ClassAttribute("description", "The distance between rays when tracing the SSR effect.") } }),
    Field(NAME(HYP_STR(NumIterations)), &SSRRendererConfig::numIterations, offsetof(SSRRendererConfig, numIterations), Span<const ClassAttribute> { {ClassAttribute("description", "The maximum number of iterations to perform for the SSR effect before stopping.") } }),
    Field(NAME(HYP_STR(EyeFade)), &SSRRendererConfig::eyeFade, offsetof(SSRRendererConfig, eyeFade), Span<const ClassAttribute> { {ClassAttribute("description", "Where to start and end fading the SSR effect based on the eye vector.") } }),
    Field(NAME(HYP_STR(ScreenEdgeFade)), &SSRRendererConfig::screenEdgeFade, offsetof(SSRRendererConfig, screenEdgeFade), Span<const ClassAttribute> { {ClassAttribute("description", "Where to start and end fading the SSR effect based on the screen edges.") } }),
    Field(NAME(HYP_STR(Extent)), &SSRRendererConfig::extent, offsetof(SSRRendererConfig, extent), Span<const ClassAttribute> { {ClassAttribute("jsonignore", true) } })
HYP_END_STRUCT

#pragma endregion SSRRendererConfig Reflection Data

} // namespace hyperion

