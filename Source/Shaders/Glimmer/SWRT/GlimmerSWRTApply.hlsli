#ifndef GLIMMER_SWRT_APPLY_HLSLI
#define GLIMMER_SWRT_APPLY_HLSLI

#include "GlimmerProbeTypes.hlsli"

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SWRT_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SWRT_APPLY_SAMPLING_HLSLI

// Bound by GlimmerTechnique::BindApplyResources()
DECLARE_SRV(DeferredPass, GlimmerProbeBlockTableBuffer) StructuredBuffer<uint> glimmerProbeBlockTable;
DECLARE_SRV(DeferredPass, GlimmerProbeSHBuffer) StructuredBuffer<float4> glimmerProbeSH;
DECLARE_SRV(DeferredPass, GlimmerProbeStatesBuffer) StructuredBuffer<uint4> glimmerProbeStates;
DECLARE_SRV(DeferredPass, GlimmerProbeVisibilityBuffer) StructuredBuffer<float4> glimmerProbeVisibility;

#include "GlimmerProbes.hlsli"

/*! The near field: irradiance / pi from the probe blocks at P, with how much they cover P as .a. */
float4 EvaluateGlimmerProbes(GlimmerProbeVolume volume, float3 P, float3 N)
{
    return SampleGlimmerProbes(volume, P + N * 0.05, N);
}

#endif
