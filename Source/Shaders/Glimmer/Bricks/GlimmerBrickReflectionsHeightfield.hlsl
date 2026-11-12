#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"

// what of a pixel's reflection the bricks left uncovered, from the heightfield: the ground, the spans' solids and the canopy
// HALF adds it to the half resolution picture before it is added up, WHOLE to the pixels of polished things that traced at full resolution
PERMUTE(MODE, HALF, WHOLE)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#include "../../Include/EnvProbes.hlsli"

DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(DeferredPass, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;

#include "../../Include/Gbuffer.hlsli"

DECLARE_SRV_DYNAMIC(DeferredPass, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(DeferredPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(DeferredPass, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(DeferredPass, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(DeferredPass, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(DeferredPass, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;
DECLARE_SRV(DeferredPass, GlimmerRelightTexture) Texture2DArray<float4> glimmerRelight;

// rgb = radiance times a, a = how much of the pixel's reflection is covered
DECLARE_UAV(DeferredPass, OutColor) RWTexture2D<float4> OutColor;
// x = how far the pixel's rays went, less than 0 where they left the bricks
DECLARE_UAV(DeferredPass, OutInfo) RWTexture2D<float4> OutInfo;

DECLARE_SRV(DeferredPass, SlotWholeBuffer) StructuredBuffer<uint> slotWhole;
DECLARE_UAV(DeferredPass, SlotCountsBuffer) RWStructuredBuffer<uint> slotCounts;

#define GLIMMER_BRICKS_NO_TRACE
#include "GlimmerBricks.hlsli"

#include "../GlimmerLight.hlsli"
#include "../GlimmerRelight.hlsli"
#include "../GlimmerApply.hlsli"

struct GlimmerBrickReflectionsHeightfieldConstants
{
    GlimmerBrickParams bricks;
    uint4 dimensions; // xy = the view's, zw = the half resolution picture's
    float4 params;    // x = the foliage's extinction
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerRelightParams relight;
    GlimmerLightParams light;
};

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    GlimmerApply glimmer;
    GlimmerBrickReflectionsHeightfieldConstants constants;
};

#define GLIMMER_APPLY_WITH_SAMPLING
#include "../GlimmerApply.hlsli"

#define GLIMMER_RELIGHT_WITH_SAMPLING
#include "../GlimmerRelight.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerBrickReflectionsSurface.hlsli"

// NOTE must match GlimmerBrickReflections.hlsl
#define COUNT_WHOLE 3
#define REACH_HOLE -2.0
#define TRACE_ROW 256u

#define HEIGHTFIELD_RAY_DISTANCE 4000.0
#define MAX_ALBEDO 0.9
#define GROUND_ALBEDO 0.3
#define LEAST_MISSING 0.01
#define SOLIDS_SLACK_TEXELS 1.5

GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo, GlimmerLight skyTransfer)
{
    return GlimmerRestLight(weightedAlbedo * max(EvaluateGlimmer(glimmer, P, float3(0.0, 1.0, 0.0)).rgb, (float3)0.0));
}

#include "../GlimmerHeightfield.hlsli"

// rgb = radiance times a, a = how much of the ray the heightfield covers; outKind is GLIMMER_HEIGHTFIELD_GROUND, _SOLID or _MISS
float4 TraceHeightfield(float3 origin, float3 direction, out float outT, out uint outKind)
{
    const float solidsFromT = max(GlimmerBrickWindowExit(constants.bricks, origin, direction), SOLIDS_SLACK_TEXELS * constants.spans.levels[0].params.x);

    GlimmerHeightfieldHit hit;

    const bool isHit = GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, HEIGHTFIELD_RAY_DISTANCE, 0u, solidsFromT, constants.params.x, true, hit);

    float3 radiance = GlimmerComposeRayLight(constants.light, hit.inscatter, direction, 0.0);
    float coverage = 1.0 - hit.transmittance;

    outT = hit.t;
    outKind = isHit && hit.t > 0.0 ? hit.kind : GLIMMER_HEIGHTFIELD_MISS;

    if (isHit && hit.t > 0.0)
    {
        const float3 P = origin + direction * hit.t;
        const bool isGround = hit.kind == GLIMMER_HEIGHTFIELD_GROUND;

        const float3 albedo = min(isGround ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, P.xz, hit.level, (float3)GROUND_ALBEDO) : hit.albedo, (float3)MAX_ALBEDO);

        GlimmerLight received;
        float3 receivedSun;
        float3 surfaceRadiance;

        if (GlimmerRelightCovers(isGround, hit.normal)
            && GlimmerSampleRelight(constants.relight, constants.ground, hit.level, P.xz, isGround ? GLIMMER_RELIGHT_GROUND : GLIMMER_RELIGHT_SPAN_TOP, received, receivedSun))
        {
            surfaceRadiance = GlimmerComposeRayLight(constants.light, GlimmerScaleLight(received, albedo), direction, 0.0);
        }
        else
        {
            surfaceRadiance = albedo * max(EvaluateGlimmer(glimmer, P + hit.normal * 0.1, hit.normal).rgb, (float3)0.0);
        }

        radiance += surfaceRadiance * hit.transmittance;
        coverage = 1.0;
    }

    return float4(max(radiance, (float3)0.0), coverage);
}

float AddHeightfield(int2 pixel, inout float4 color)
{
    const ReflectionSurface surface = LoadSurface(pixel);

    if (!surface.isValid)
    {
        return 0.0;
    }

    const float reflective = 1.0 - MatteShare(surface.roughness);
    const float missing = reflective - color.a;

    if (missing < LEAST_MISSING)
    {
        return 0.0;
    }

    const float3 surfaceNormal = LoadSurfaceNormal(pixel, surface.P, surface.N);

    float t;
    uint kind;
    const float4 found = TraceHeightfield(RayStart(surface.P, surfaceNormal), Reflected(surface, surfaceNormal), t, kind);

    if (found.a <= 0.0)
    {
        return 0.0;
    }

    color += float4(found.rgb, found.a) * missing;

    return t;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
#if defined(MODE_HALF)
    const uint2 texel = dispatchThreadId.xy;

    if (any(texel >= constants.dimensions.zw))
    {
        return;
    }

    float4 info = OutInfo[texel];

    // no ray this frame: what it had before stands in
    if (info.x == REACH_HOLE)
    {
        return;
    }

    float4 color = OutColor[texel];

    const float t = AddHeightfield(FullPixel(int2(texel)), color);

    if (t > 0.0)
    {
        OutColor[texel] = color;

        // the history follows the reflection by how far it is
        if (info.x < 0.0)
        {
            info.x = t;
            OutInfo[texel] = info;
        }
    }
#else
    const uint listed = dispatchThreadId.y * TRACE_ROW + dispatchThreadId.x;

    if (listed >= slotCounts[COUNT_WHOLE])
    {
        return;
    }

    const uint place = slotWhole[listed];
    const int2 pixel = int2(place % constants.dimensions.x, place / constants.dimensions.x);

    float4 color = OutColor[pixel];

    if (AddHeightfield(pixel, color) > 0.0)
    {
        OutColor[pixel] = color;
    }
#endif
}
