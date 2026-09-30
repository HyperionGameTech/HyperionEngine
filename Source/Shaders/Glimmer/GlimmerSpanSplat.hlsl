#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../Include/RayTracing/BVH.hlsli"
#include "GlimmerCommon.hlsli"

PERMUTE(MODE, CLEAR, SPLAT)

// Must match GlimmerSpanMaxRects in GlimmerSpanCache.cpp
#define GLIMMER_SPAN_MAX_RECTS 4

// Must match GlimmerSpanSplatConstants in GlimmerSpanCache.cpp
struct GlimmerSpanSplatConstants
{
    int4 window;   // xy = absolute texel of the window origin, z = level, w = rects to fill
    float4 params; // x = texel size, y = 1 / texel size, z = minimum height of foliage above the ground
    uint4 counts;  // x = span instances, y = span chunks, z = groups along x
    int4 rects[GLIMMER_SPAN_MAX_RECTS]; // absolute texels, xy = min, zw = max (exclusive), inside the window; only these are cleared and filled
    GlimmerGroundParams ground;
};

DECLARE_BUFFER_DYNAMIC(GlimmerSpanSplat, CBuffer) cbuffer CBuffer
{
    GlimmerSpanSplatConstants constants;
};

DECLARE_UAV(GlimmerSpanSplat, SpansBuffer) RWStructuredBuffer<uint> spans;
DECLARE_SRV(GlimmerSpanSplat, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerSpanSplat, SpanChunksBuffer) StructuredBuffer<GlimmerSpanChunk> spanChunks;
DECLARE_SRV(GlimmerSpanSplat, GlimmerBLASTrianglesBuffer) StructuredBuffer<BVHTriangle> glimmerBLASTriangles;
DECLARE_SRV(GlimmerSpanSplat, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSpanSplat, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerSpanSplat, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;

#include "GlimmerMaterial.hlsli"

#include "GlimmerGround.hlsli"

#define GROUP_SIZE 64

// a triangle bigger than this many texels on a side (a big floor or wall) is only splatted this far from its corner
#define MAX_FOOTPRINT_TEXELS 64

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

bool IsInRects(int2 texel)
{
    for (int rectIndex = 0; rectIndex < constants.window.w; rectIndex++)
    {
        const int4 rect = constants.rects[rectIndex];

        if (all(texel >= rect.xy) && all(texel < rect.zw))
        {
            return true;
        }
    }

    return false;
}

void AccumulateTexel(uint baseIndex, bool isFoliage, uint minY, uint maxY, uint areaFixed, uint3 albedoFixed)
{
    uint previous;

    if (isFoliage)
    {
        InterlockedMin(spans[baseIndex + GLIMMER_SPAN_CANOPY_MIN], minY, previous);
        InterlockedMax(spans[baseIndex + GLIMMER_SPAN_CANOPY_MAX], maxY, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_LEAF_AREA], areaFixed, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 0], albedoFixed.r, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 1], albedoFixed.g, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 2], albedoFixed.b, previous);
    }
    else
    {
        InterlockedMin(spans[baseIndex + GLIMMER_SPAN_SOLID_MIN], minY, previous);
        InterlockedMax(spans[baseIndex + GLIMMER_SPAN_SOLID_MAX], maxY, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 0], albedoFixed.r, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 1], albedoFixed.g, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 2], albedoFixed.b, previous);
        InterlockedAdd(spans[baseIndex + GLIMMER_SPAN_SOLID_AREA], areaFixed, previous);
    }
}

void SplatTriangle(GlimmerSpanInstance instance, uint localTriangle, uint level)
{
    const BVHTriangle bvhTriangle = glimmerBLASTriangles[instance.data.x + localTriangle];

    const float3 p0 = TransformPoint(instance, bvhTriangle.position0.xyz);
    const float3 p1 = TransformPoint(instance, bvhTriangle.position0.xyz + bvhTriangle.edge1.xyz);
    const float3 p2 = TransformPoint(instance, bvhTriangle.position0.xyz + bvhTriangle.edge2.xyz);

    const float texelSize = constants.params.x;
    const float invTexelSize = constants.params.y;

    const int2 windowMin = constants.window.xy;
    const int2 windowMax = windowMin + GLIMMER_GROUND_RESOLUTION;

    const float2 boundsMin = min(p0.xz, min(p1.xz, p2.xz));
    const float2 boundsMax = max(p0.xz, max(p1.xz, p2.xz));

    const int2 texelMin = max(int2(floor(boundsMin * invTexelSize)), windowMin);
    const int2 texelMax = min(int2(floor(boundsMax * invTexelSize)), min(windowMax - 1, texelMin + MAX_FOOTPRINT_TEXELS - 1));

    if (any(texelMax < texelMin))
    {
        return;
    }

    bool overlapsRects = false;

    for (int rectIndex = 0; rectIndex < constants.window.w; rectIndex++)
    {
        const int4 rect = constants.rects[rectIndex];

        overlapsRects = overlapsRects || (all(texelMax >= rect.xy) && all(texelMin < rect.zw));
    }

    if (!overlapsRects)
    {
        return;
    }

    const bool isFoliage = (instance.data.w & GLIMMER_INSTANCE_FLAG_FOLIAGE) != 0u;

    // grass and ground cover hug the terrain, far thinner than the probes above it can resolve; screen space and material AO cover them
    if (isFoliage)
    {
        const float3 triangleCenter = (p0 + p1 + p2) / 3.0;

        float groundHeight;
        uint groundLevel;

        if (GlimmerSampleGround(constants.ground, triangleCenter.xz, uint(constants.window.z), groundHeight, groundLevel)
            && max(p0.y, max(p1.y, p2.y)) < groundHeight + constants.params.z)
        {
            return;
        }
    }

    const float4 albedoAlpha = GlimmerGetMaterialAverageAlbedoAlpha(instance.data.z);

    // surface area per texel area, spread evenly over the texels the triangle's bounds touch: for leaves it's the leaf area
    // index, for solids how filled the texel is, so walls and trunks become solid columns but a few thin branches don't
    const float3 edgeA = p1 - p0;
    const float3 edgeB = p2 - p0;
    const float surfaceArea = 0.5 * length(cross(edgeA, edgeB)) * (isFoliage ? saturate(albedoAlpha.a) : 1.0);

    const uint2 footprint = uint2(texelMax - texelMin) + 1u;
    const float texelArea = texelSize * texelSize;

    const float areaPerTexel = surfaceArea / (float(footprint.x * footprint.y) * texelArea);
    const uint areaFixed = uint(min(areaPerTexel * GLIMMER_SPAN_AREA_SCALE, 65535.0) + 0.5);

    if (areaFixed == 0u)
    {
        return;
    }

    const uint minY = GlimmerOrderedUintFromFloat(min(p0.y, min(p1.y, p2.y)));
    const uint maxY = GlimmerOrderedUintFromFloat(max(p0.y, max(p1.y, p2.y)));

    const uint3 albedoFixed = uint3(saturate(albedoAlpha.rgb) * float(areaFixed) + 0.5);

    for (int rectIndex = 0; rectIndex < constants.window.w; rectIndex++)
    {
        const int4 rect = constants.rects[rectIndex];

        const int2 rectMin = max(texelMin, rect.xy);
        const int2 rectMax = min(texelMax, rect.zw - 1);

        for (int z = rectMin.y; z <= rectMax.y; z++)
        {
            for (int x = rectMin.x; x <= rectMax.x; x++)
            {
                AccumulateTexel(GlimmerSpanTexelIndex(level, int2(x, z)), isFoliage, minY, maxY, areaFixed, albedoFixed);
            }
        }
    }
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint threadIndex = (groupId.y * constants.counts.z + groupId.x) * GROUP_SIZE + groupIndex;
    const uint level = uint(constants.window.z);

#if defined(MODE_CLEAR)
    if (threadIndex >= GLIMMER_GROUND_RESOLUTION * GLIMMER_GROUND_RESOLUTION)
    {
        return;
    }

    const int2 texel = constants.window.xy + int2(threadIndex % GLIMMER_GROUND_RESOLUTION, threadIndex / GLIMMER_GROUND_RESOLUTION);

    if (!IsInRects(texel))
    {
        return;
    }

    const uint baseIndex = GlimmerSpanTexelIndex(level, texel);

    spans[baseIndex + GLIMMER_SPAN_SOLID_MIN] = GLIMMER_MASK_EMPTY_MIN;
    spans[baseIndex + GLIMMER_SPAN_SOLID_MAX] = GLIMMER_MASK_EMPTY_MAX;
    spans[baseIndex + GLIMMER_SPAN_CANOPY_MIN] = GLIMMER_MASK_EMPTY_MIN;
    spans[baseIndex + GLIMMER_SPAN_CANOPY_MAX] = GLIMMER_MASK_EMPTY_MAX;

    [unroll]
    for (uint valueIndex = GLIMMER_SPAN_LEAF_AREA; valueIndex < GLIMMER_SPAN_VALUES_PER_TEXEL; valueIndex++)
    {
        spans[baseIndex + valueIndex] = 0u;
    }
#elif defined(MODE_SPLAT)
    const uint chunkIndex = groupId.y * constants.counts.z + groupId.x;

    if (chunkIndex >= constants.counts.y)
    {
        return;
    }

    const GlimmerSpanChunk chunk = spanChunks[chunkIndex];

    // the whole chunk goes when its instance misses every rect being filled
    bool overlapsRects = false;

    for (int rectIndex = 0; rectIndex < constants.window.w; rectIndex++)
    {
        const float4 rectBounds = float4(constants.rects[rectIndex]) * constants.params.x;

        overlapsRects = overlapsRects || (all(chunk.boundsMax.xz >= rectBounds.xy) && all(chunk.boundsMin.xz <= rectBounds.zw));
    }

    if (!overlapsRects)
    {
        return;
    }

    const GlimmerSpanInstance instance = spanInstances[asuint(chunk.boundsMin.w)];

    for (uint chunkTriangle = groupIndex; chunkTriangle < GLIMMER_SPAN_CHUNK_TRIANGLES; chunkTriangle += GROUP_SIZE)
    {
        const uint localTriangle = asuint(chunk.boundsMax.w) + chunkTriangle;

        if (localTriangle >= instance.data.y)
        {
            break;
        }

        SplatTriangle(instance, localTriangle, level);
    }
#endif
}
