#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region SSRRendererConfig Reflection Data

HYP_BEGIN_STRUCT(SSRRendererConfig, 323, 0, {}, HypClassAttribute("configname", "GlobalConfig"),HypClassAttribute("jsonpath", "Rendering.SSR"))
    HypField(NAME(HYP_STR(Enabled)), &SSRRendererConfig::enabled, offsetof(SSRRendererConfig, enabled)),
    HypField(NAME(HYP_STR(Quality)), &SSRRendererConfig::quality, offsetof(SSRRendererConfig, quality), Span<const HypClassAttribute> { {HypClassAttribute("description", "The quality level of the SSR effect. 0 = low, 1 = medium, 2 = high") } }),
    HypField(NAME(HYP_STR(RoughnessScattering)), &SSRRendererConfig::roughnessScattering, offsetof(SSRRendererConfig, roughnessScattering), Span<const HypClassAttribute> { {HypClassAttribute("description", "Enables scattering of rays based on the roughness of the surface. May cause artifacts due to temporal instability.") } }),
    HypField(NAME(HYP_STR(ConeTracing)), &SSRRendererConfig::coneTracing, offsetof(SSRRendererConfig, coneTracing), Span<const HypClassAttribute> { {HypClassAttribute("description", "Enables cone tracing for the SSR effect. Causes the result to become blurrier based on distance of the reflection.") } }),
    HypField(NAME(HYP_STR(RayStep)), &SSRRendererConfig::rayStep, offsetof(SSRRendererConfig, rayStep), Span<const HypClassAttribute> { {HypClassAttribute("description", "The distance between rays when tracing the SSR effect.") } }),
    HypField(NAME(HYP_STR(NumIterations)), &SSRRendererConfig::numIterations, offsetof(SSRRendererConfig, numIterations), Span<const HypClassAttribute> { {HypClassAttribute("description", "The maximum number of iterations to perform for the SSR effect before stopping.") } }),
    HypField(NAME(HYP_STR(EyeFade)), &SSRRendererConfig::eyeFade, offsetof(SSRRendererConfig, eyeFade), Span<const HypClassAttribute> { {HypClassAttribute("description", "Where to start and end fading the SSR effect based on the eye vector.") } }),
    HypField(NAME(HYP_STR(ScreenEdgeFade)), &SSRRendererConfig::screenEdgeFade, offsetof(SSRRendererConfig, screenEdgeFade), Span<const HypClassAttribute> { {HypClassAttribute("description", "Where to start and end fading the SSR effect based on the screen edges.") } }),
    HypField(NAME(HYP_STR(Extent)), &SSRRendererConfig::extent, offsetof(SSRRendererConfig, extent), Span<const HypClassAttribute> { {HypClassAttribute("jsonignore", true) } })
HYP_END_STRUCT

#pragma endregion SSRRendererConfig Reflection Data

} // namespace hyperion

