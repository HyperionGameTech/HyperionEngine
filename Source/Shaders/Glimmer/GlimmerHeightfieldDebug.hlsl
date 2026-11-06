#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerCommon.hlsli"

// its types now, its tracing once the texture is declared
#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "SH/GlimmerSHOccupancy.hlsli"
#undef GLIMMER_SH_OCCUPANCY_NO_TRACE

struct GlimmerHeightfieldDebugConstants
{
    uint4 dimensionsMode; // xy = output size, z = debug view
    float4 params;        // x = max trace distance
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerSHOccupancyParams occupancy;
};

#define DEBUG_VIEW_SPANS 1
#define DEBUG_VIEW_GROUND_ALBEDO 2
#define DEBUG_VIEW_OCCUPANCY 3

DECLARE_BUFFER_DYNAMIC(GlimmerHeightfieldDebug, CBuffer) cbuffer CBuffer
{
    GlimmerHeightfieldDebugConstants constants;
};

DECLARE_UAV(GlimmerHeightfieldDebug, OutImage) RWTexture2D<float4> OutImage;

DECLARE_SRV(GlimmerHeightfieldDebug, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(GlimmerHeightfieldDebug, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;

DECLARE_SRV_DYNAMIC(GlimmerHeightfieldDebug, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(GlimmerHeightfieldDebug, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerHeightfieldDebug, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerHeightfieldDebug, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerHeightfieldDebug, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerHeightfieldDebug, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;
DECLARE_SRV(GlimmerHeightfieldDebug, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerHeightfieldDebug, GlimmerSHOccupancyTexture) Texture3D<float4> glimmerSHOccupancy;
DECLARE_SRV(GlimmerHeightfieldDebug, GlimmerSHOccupancyMaskBuffer) StructuredBuffer<uint> glimmerSHOccupancyMask;

#include "SH/GlimmerSHOccupancy.hlsli"

#include "GlimmerMaterial.hlsli"

#include "GlimmerLight.hlsli"

GlimmerLight GlimmerCanopyRadiance(
    float3 P,
    float3 weightedAlbedo,
    GlimmerLight skyTransfer)
{
    return GlimmerRestLight(weightedAlbedo);
}

#include "GlimmerHeightfield.hlsli"

#include "GlimmerDebug.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 dimensions = constants.dimensionsMode.xy;
    const uint2 coord = dispatchThreadId.xy;

    if (any(coord >= dimensions))
    {
        return;
    }

    const uint mode = constants.dimensionsMode.z;

    const float2 uv = (float2(coord) + 0.5) / float2(dimensions);

    const float3 cameraPosition = camera.position.xyz;
    const float3 farPosition = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, uv, 1.0).xyz;
    const float3 direction = normalize(farPosition - cameraPosition);

    float4 result = float4(SkyColor(direction), 1.0);

    if (mode == DEBUG_VIEW_OCCUPANCY)
    {
        GlimmerSHOccupancyHit occupancyHit;
        float coveredT;

        result.rgb = SkyColor(direction);

        if (GlimmerSHTraceOccupancy(constants.occupancy, cameraPosition, direction, constants.params.x, occupancyHit, coveredT))
        {
            const float4 lightmap = GlimmerSHLoadHitLightmap(occupancyHit, direction);

            result.rgb = lightmap.a > 0.0 ? occupancyHit.albedo * lightmap.rgb : ShadeHit(occupancyHit.albedo, occupancyHit.normal);
        }
    }
    else if (mode == DEBUG_VIEW_SPANS || mode == DEBUG_VIEW_GROUND_ALBEDO)
    {
        const float depth = GBufferDepthTexture.Load(int3(coord, 0)).r;
        const bool hasRasterSurface = depth < 1.0;

        const float3 rasterPosition = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, uv, depth).xyz;

        // same packing as GBufferUnpackNormal()
        const float3 rasterNormal = normalize(DecodeNormal(GBufferNormalsTexture.Load(int3(coord, 0)).yzww));
        const float grey = hasRasterSurface ? 0.25 + 0.5 * saturate(dot(rasterNormal, DebugLightDirection) * 0.5 + 0.5) : 0.15;

        if (mode == DEBUG_VIEW_GROUND_ALBEDO)
        {
            // the albedo probes see on the ground under each surface, from the finest level that has it; dark magenta: none yet
            result.rgb = hasRasterSurface ? float3(0.25, 0.0, 0.25) : SkyColor(direction) * 0.5;

            [loop]
            for (uint level = 0; level < GLIMMER_GROUND_LEVELS && hasRasterSurface; level++)
            {
                const GlimmerGroundLevel groundLevel = constants.ground.levels[level];
                const int2 texel = int2(floor(rasterPosition.xz * groundLevel.params.y));

                if (any(texel < groundLevel.validRect.xy) || any(texel >= groundLevel.validRect.zw))
                {
                    continue;
                }

                const float4 albedo = glimmerGroundAlbedo.Load(int4(GlimmerWrapGroundTexel(texel), level, 0));

                if (albedo.a > 0.5)
                {
                    result.rgb = albedo.rgb;

                    break;
                }
            }
        }
        else
        {
            result.rgb = float3(grey, grey, grey) * 0.5;

            GlimmerSpanSample spanSample;

            if (hasRasterSurface && GlimmerSampleSpans(constants.spans, 0u, rasterPosition.xz, spanSample))
            {
                const bool inCanopy = GlimmerSpanOccupies(rasterPosition.y, 0.5, spanSample.canopyMin, spanSample.canopyMax, spanSample.canopyBins);
                const bool inSolid = GlimmerSpanOccupies(rasterPosition.y, 0.5, spanSample.solidMin, spanSample.solidMax, spanSample.solidBins);

                float leafArea = 0.0;

                for (uint band = 0; band < GLIMMER_SPAN_BANDS; band++)
                {
                    leafArea += GlimmerSpanBandArea(spanSample.canopyBands, band);
                }

                result.g += saturate(leafArea / 4.0) * (inCanopy ? 1.0 : 0.35);
                result.r += saturate(GlimmerSpanSolidFillAt(spanSample, rasterPosition.y, constants.spans.levels[0].params.x) / GLIMMER_SPAN_SOLID_THRESHOLD) * (inSolid ? 1.0 : 0.35);
            }

            float groundHeight;
            uint groundLevel;

            if (hasRasterSurface && GlimmerSampleGround(constants.ground, rasterPosition.xz, 0u, groundHeight, groundLevel) && abs(rasterPosition.y - groundHeight) < 0.5)
            {
                result.b += 0.35;
            }
        }
    }

    OutImage[coord] = result;
}
