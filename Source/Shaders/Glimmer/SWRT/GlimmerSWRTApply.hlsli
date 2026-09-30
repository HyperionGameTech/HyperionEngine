#ifndef GLIMMER_SWRT_APPLY_HLSLI
#define GLIMMER_SWRT_APPLY_HLSLI

#include "GlimmerProbeTypes.hlsli"

// Must match what GlimmerSWRTTechnique::WriteApplyShaderData() writes
struct GlimmerTechniqueApply
{
    GlimmerProbeVolume volume;
};

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SWRT_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SWRT_APPLY_SAMPLING_HLSLI

// Bound by GlimmerSWRTTechnique::BindApplyResources()
DECLARE_SRV(DeferredPass, GlimmerProbeSH0Texture) Texture3D<float4> glimmerProbeSH0;
DECLARE_SRV(DeferredPass, GlimmerProbeSH1Texture) Texture3D<float4> glimmerProbeSH1;
DECLARE_SRV(DeferredPass, GlimmerProbeSH2Texture) Texture3D<float4> glimmerProbeSH2;
DECLARE_SRV(DeferredPass, GlimmerProbeStateTexture) Texture3D<uint2> glimmerProbeState;
DECLARE_SRV(DeferredPass, GlimmerProbeBaseTexture) Texture2DArray<float> glimmerProbeBase;

#include "GlimmerProbes.hlsli"

float4 EvaluateGlimmerTechnique(GlimmerTechniqueApply techniqueApply, float3 P, float3 N)
{
    return SampleGlimmerProbes(techniqueApply.volume, P + N * 0.05, N);
}

#endif
