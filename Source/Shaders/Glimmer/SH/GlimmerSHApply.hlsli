#ifndef GLIMMER_SH_APPLY_HLSLI
#define GLIMMER_SH_APPLY_HLSLI

#include "GlimmerSHCommon.hlsli"

#endif

#if defined(GLIMMER_APPLY_WITH_SAMPLING) && !defined(GLIMMER_SH_APPLY_SAMPLING_HLSLI)
#define GLIMMER_SH_APPLY_SAMPLING_HLSLI

#ifndef GLIMMER_SH_APPLY_EXTERNAL_RESOURCES
DECLARE_SRV(DeferredPass, GlimmerSHDataTexture) Texture3D<float4> glimmerSHData;
DECLARE_SRV(DeferredPass, GlimmerSHStateTexture) Texture3D<uint4> glimmerSHState;
DECLARE_SRV(DeferredPass, GlimmerSHRadianceTexture) Texture3D<float4> glimmerSHRadiance;
#endif

#ifndef GLIMMER_SH_LOAD_DATA
#define GLIMMER_SH_LOAD_DATA(texel) glimmerSHData.Load(int4(texel, 0))
#define GLIMMER_SH_LOAD_STATE(texel) glimmerSHState.Load(int4(texel, 0))
#define GLIMMER_SH_LOAD_RADIANCE(texel) glimmerSHRadiance.Load(int4(texel, 0))
#endif

#include "GlimmerSHSample.hlsli"

float GlimmerSHNearestSkySeen(GlimmerSHVolume volume, float3 P, float3 N)
{
    [loop]
    for (uint cascadeIndex = 0; cascadeIndex < GLIMMER_SH_CASCADES; cascadeIndex++)
    {
        const GlimmerSHCascade cascade = volume.cascades[cascadeIndex];

        const int3 voxel = int3(floor(P * cascade.params.y));
        const int3 local = voxel - cascade.origin.xyz;

        if (cascade.origin.w == 0 || any(local < 1) || any(local >= int3(GLIMMER_SH_GRID_XZ, GLIMMER_SH_GRID_Y, GLIMMER_SH_GRID_XZ) - 1))
        {
            continue;
        }

        const uint3 texel = GlimmerSHTexel(cascadeIndex, voxel);

        GlimmerSHVoxel voxelData;

        if (!GlimmerSHUnpackVoxel(GLIMMER_SH_LOAD_STATE(texel), voxel, voxelData) || voxelData.isBuried)
        {
            return -1.0;
        }

        const float4 visibility = GLIMMER_SH_LOAD_DATA(texel);

        return saturate(visibility.x + dot(visibility.yzw, N));
    }

    return -1.0;
}

float3 GlimmerSHEvaluate(GlimmerSHRadiance radiance, float3 N)
{
    return GlimmerEvaluateL1(radiance.r, radiance.g, radiance.b, N);
}

float4 EvaluateGlimmerSH(GlimmerSHVolume volume, float3 P, float3 N)
{
    float4 visibility;
    GlimmerSHRadiance radiance;
    const float coverage = GlimmerSHSampleVolume(volume, P, N, visibility, radiance);

    if (coverage <= 0.0)
    {
        return (float4)0.0;
    }

    return float4(GlimmerSHEvaluate(radiance, N), coverage);
}

// returns:
//   x = the sky a reflection along R sees, relative to open ground
//   y = how much that can be trusted (none looking down, where open ground sees no sky either)
float2 GlimmerSHSpecularVisibility(float4 visibility, float coverage, float3 R)
{
    const float openSky = 0.5 + 0.5 * R.y;
    const float skySeen = saturate(visibility.x + dot(visibility.yzw, R));

    return float2(saturate(skySeen / max(openSky, 1e-3)), coverage * saturate(openSky * 4.0));
}

#endif
