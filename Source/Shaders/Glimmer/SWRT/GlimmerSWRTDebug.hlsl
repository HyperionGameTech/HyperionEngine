#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"
#include "../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../Include/RayTracing/BVH.hlsli"
#include "GlimmerCommon.hlsli"

// Must match GlimmerSWRTDebugConstants in GlimmerPass.cpp
struct GlimmerSWRTDebugConstants
{
    uint4 dimensionsModeInstances; // xy = output size, z = debug view, w = number of instances
    GlimmerFootprintMaskParams mask;
    float4 params;                 // x = max trace distance
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
};

#define DEBUG_VIEW_SHADED 1
#define DEBUG_VIEW_INSTANCE_ID 2
#define DEBUG_VIEW_TRAVERSAL_COST 3
#define DEBUG_VIEW_DEPTH_COMPARE 4
#define DEBUG_VIEW_FOOTPRINT_MASK 5
#define DEBUG_VIEW_SPANS 6
#define DEBUG_VIEW_GROUND_ALBEDO 7

DECLARE_BUFFER_DYNAMIC(GlimmerSWRTDebug, CBuffer) cbuffer CBuffer
{
    GlimmerSWRTDebugConstants constants;
};

DECLARE_UAV(GlimmerSWRTDebug, OutImage) RWTexture2D<float4> OutImage;

DECLARE_SRV(GlimmerSWRTDebug, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(GlimmerSWRTDebug, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;

DECLARE_SRV_DYNAMIC(GlimmerSWRTDebug, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(GlimmerSWRTDebug, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSWRTDebug, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerSWRTDebug, GlimmerTLASNodesBuffer) StructuredBuffer<BVHNode> glimmerTLASNodes;
DECLARE_SRV(GlimmerSWRTDebug, GlimmerInstancesBuffer) StructuredBuffer<GlimmerInstance> glimmerInstances;
DECLARE_SRV(GlimmerSWRTDebug, GlimmerBLASNodesBuffer) StructuredBuffer<BVHNode> glimmerBLASNodes;
DECLARE_SRV(GlimmerSWRTDebug, GlimmerBLASTrianglesBuffer) StructuredBuffer<BVHTriangle> glimmerBLASTriangles;
DECLARE_SRV(GlimmerSWRTDebug, FootprintMaskBuffer) StructuredBuffer<uint> footprintMask;
DECLARE_SRV(GlimmerSWRTDebug, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerSWRTDebug, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerSWRTDebug, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;

#include "GlimmerSWRT.hlsli"
#include "GlimmerMaterial.hlsli"

float3 GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    return albedo;
}

#include "GlimmerHeightfield.hlsli"

static const float3 DebugLightDirection = normalize(float3(0.45, 0.8, 0.35));

float3 HashColor(uint value)
{
    uint hash = value * 747796405u + 2891336453u;
    hash = ((hash >> ((hash >> 28u) + 4u)) ^ hash) * 277803737u;
    hash = (hash >> 22u) ^ hash;

    return float3(float(hash & 0xFFu), float((hash >> 8u) & 0xFFu), float((hash >> 16u) & 0xFFu)) / 255.0 * 0.8 + 0.2;
}

// Turbo-like ramp, 0 = cold, 1 = hot
float3 HeatColor(float value)
{
    const float t = saturate(value);

    const float3 cold = float3(0.05, 0.1, 0.45);
    const float3 mid = float3(0.1, 0.85, 0.35);
    const float3 hot = float3(0.95, 0.15, 0.05);

    return t < 0.5 ? lerp(cold, mid, t * 2.0) : lerp(mid, float3(1.0, 0.85, 0.1), (t - 0.5) * 2.0) * (t > 0.9 ? lerp(1.0, hot / float3(1.0, 0.85, 0.1), (t - 0.9) * 10.0) : 1.0);
}

float3 ShadeHit(float3 albedo, float3 normal)
{
    const float lambert = saturate(dot(normal, DebugLightDirection));
    const float skyTerm = 0.5 + 0.5 * normal.y;

    return albedo * (0.2 * skyTerm + 0.8 * lambert);
}

float3 SkyColor(float3 direction)
{
    return lerp(float3(0.35, 0.4, 0.45), float3(0.15, 0.25, 0.45), saturate(direction.y));
}

bool SampleFootprintMask(float3 position, out float minY, out float maxY)
{
    minY = 0.0;
    maxY = 0.0;

    uint2 cell;

    if (constants.mask.originCellSize.w == 0.0 || !GlimmerMaskWorldToCell(constants.mask, 0u, position.xz, cell))
    {
        return false;
    }

    const uint index = GlimmerMaskCellIndex(constants.mask, 0u, cell);
    const uint encodedMinY = footprintMask[index];
    const uint encodedMaxY = footprintMask[index + 1u];

    if (encodedMinY > encodedMaxY)
    {
        return false;
    }

    minY = GlimmerFloatFromOrderedUint(encodedMinY);
    maxY = GlimmerFloatFromOrderedUint(encodedMaxY);

    return true;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 dimensions = constants.dimensionsModeInstances.xy;
    const uint2 coord = dispatchThreadId.xy;

    if (any(coord >= dimensions))
    {
        return;
    }

    const uint mode = constants.dimensionsModeInstances.z;
    const uint numInstances = constants.dimensionsModeInstances.w;

    const float2 uv = (float2(coord) + 0.5) / float2(dimensions);

    const float3 cameraPosition = camera.position.xyz;
    const float3 farPosition = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, uv, 1.0).xyz;
    const float3 direction = normalize(farPosition - cameraPosition);

    GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
    GlimmerSWRTHit hit;

    const bool didHit = TraceGlimmerSWRT(cameraPosition, direction, 0.0, constants.params.x, numInstances, false, hit, stats);

    float3 hitColor = SkyColor(direction);
    float3 hitNormal = -direction;

    if (didHit)
    {
        hitNormal = GlimmerGetHitNormal(hit, direction);

        const GlimmerInstance instance = glimmerInstances[hit.instanceIndex];

        hitColor = ShadeHit(GlimmerGetMaterialAverageAlbedo(instance.data.z), hitNormal);

        // back faces of one sided geometry, which probes inside solids would see
        if (!hit.frontFace && (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) == 0u)
        {
            hitColor = lerp(hitColor, float3(1.0, 0.0, 1.0), 0.6);
        }
    }

    float4 result = float4(hitColor, 1.0);

    if (mode == DEBUG_VIEW_INSTANCE_ID)
    {
        result.rgb = didHit
            ? HashColor(hit.instanceIndex) * (0.35 + 0.65 * saturate(dot(hitNormal, DebugLightDirection) * 0.5 + 0.5))
            : SkyColor(direction) * 0.5;
    }
    else if (mode == DEBUG_VIEW_TRAVERSAL_COST)
    {
        const float cost = float(stats.nodeVisits) + 2.0 * float(stats.triangleTests) + 8.0 * float(stats.instanceVisits);

        result.rgb = HeatColor(cost / 600.0);

        if (stats.stackOverflows != 0u)
        {
            result.rgb = float3(1.0, 1.0, 1.0);
        }
    }
    else if (mode == DEBUG_VIEW_DEPTH_COMPARE || mode == DEBUG_VIEW_FOOTPRINT_MASK || mode == DEBUG_VIEW_SPANS || mode == DEBUG_VIEW_GROUND_ALBEDO)
    {
        const float depth = GBufferDepthTexture.Load(int3(coord, 0)).r;
        const bool hasRasterSurface = depth < 1.0;

        const float3 rasterPosition = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, uv, depth).xyz;
        const float rasterDistance = length(rasterPosition - cameraPosition);

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
        else if (mode == DEBUG_VIEW_SPANS)
        {
            // green: canopy leaf area over this surface's texel (bright where the surface is inside the slab),
            // red: solidity, blue tint: the ground heightfield is within half a meter of the surface
            result.rgb = float3(grey, grey, grey) * 0.5;

            GlimmerSpanSample spanSample;

            if (hasRasterSurface && GlimmerSampleSpans(constants.spans, 0u, rasterPosition.xz, spanSample))
            {
                const bool inCanopy = rasterPosition.y >= spanSample.canopyMin - 0.5 && rasterPosition.y <= spanSample.canopyMax + 0.5;
                const bool inSolid = rasterPosition.y >= spanSample.solidMin - 0.5 && rasterPosition.y <= spanSample.solidMax + 0.5;

                result.g += saturate(spanSample.leafArea / 4.0) * (inCanopy ? 1.0 : 0.35);
                result.r += saturate(GlimmerSpanSolidFill(spanSample, constants.spans.levels[0].params.x) / GLIMMER_SPAN_SOLID_THRESHOLD) * (inSolid ? 1.0 : 0.35);
            }

            float groundHeight;
            uint groundLevel;

            if (hasRasterSurface && GlimmerSampleGround(constants.ground, rasterPosition.xz, 0u, groundHeight, groundLevel) && abs(rasterPosition.y - groundHeight) < 0.5)
            {
                result.b += 0.35;
            }
        }
        else if (mode == DEBUG_VIEW_DEPTH_COMPARE)
        {
            // green: matches raster, yellow: SWRT sees through (terrain, foliage and dynamic objects aren't in it),
            // red: SWRT hits something raster doesn't, blue: sky in both
            const float tolerance = 0.05 + rasterDistance * 0.01;

            if (!hasRasterSurface)
            {
                result.rgb = didHit ? float3(1.0, 0.1, 0.1) : float3(0.1, 0.15, 0.35);
            }
            else if (!didHit || hit.t > rasterDistance + tolerance)
            {
                result.rgb = float3(0.9, 0.8, 0.2) * grey;
            }
            else if (hit.t < rasterDistance - tolerance)
            {
                result.rgb = float3(1.0, 0.1, 0.1) * (0.5 + 0.5 * grey);
            }
            else
            {
                result.rgb = float3(0.15, 0.9, 0.25) * grey * 1.6;
            }
        }
        else
        {
            // red: surface inside an occupied cell's height range, orange: occupied cell above or below, grey: empty
            result.rgb = float3(grey, grey, grey);

            float minY;
            float maxY;

            if (hasRasterSurface && SampleFootprintMask(rasterPosition, minY, maxY))
            {
                const bool insideSpan = rasterPosition.y >= minY - 0.25 && rasterPosition.y <= maxY + 0.25;

                result.rgb = lerp(result.rgb, insideSpan ? float3(1.0, 0.15, 0.1) : float3(1.0, 0.55, 0.1), 0.6);
            }

            uint2 cell;

            if (hasRasterSurface && constants.mask.originCellSize.w != 0.0 && !GlimmerMaskWorldToCell(constants.mask, 0u, rasterPosition.xz, cell))
            {
                result.rgb *= 0.35;
            }
        }
    }

    OutImage[coord] = result;
}
