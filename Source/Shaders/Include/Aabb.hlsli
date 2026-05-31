#ifndef HYP_AABB
#define HYP_AABB

static const float3 aabb_corners[8] = {
    float3(0.0, 0.0, 0.0),
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(1.0, 1.0, 0.0),
    float3(0.0, 1.0, 1.0),
    float3(1.0, 0.0, 1.0),
    float3(1.0, 1.0, 1.0)
};

struct AABB
{
    float3 min;
    float3 max;
};

bool AABBContainsPoint(AABB aabb, float3 vec)
{
    return (
        vec.x >= aabb.min.x && vec.y >= aabb.min.y && vec.z >= aabb.min.z
        && vec.x <= aabb.max.x && vec.y <= aabb.max.y && vec.z <= aabb.max.z
    );
}

float3 AABBGetCenter(AABB aabb)
{
    return (aabb.max + aabb.min) * 0.5;
}

float3 AABBGetExtent(AABB aabb)
{
    return aabb.max - aabb.min;
}

float AABBGetGreatestExtent(AABB aabb)
{
    float3 extent = AABBGetExtent(aabb);

    return max(extent.x, max(extent.y, extent.z));
}
// https://github.com/nvpro-samples/gl_occlusion_culling/blob/master/cull-common.h
float3 AABBGetCorner(AABB aabb, int index)
{
    const float3 extent = AABBGetExtent(aabb);

    return aabb.min + aabb_corners[index] * extent;

    /*const float3 corners[8] = {
        aabb.min,
        aabb.min + float3(extent.x, 0.f, 0.f),
        aabb.min + float3(0.f, extent.y, 0.f),
        aabb.min + float3(0.f, 0.f, extent.z),
        aabb.min + float3(extent.xy, 0.f),
        aabb.min + float3(0.f, extent.yz),
        aabb.min + float3(extent.x, 0.f, extent.z),
        aabb.max
    };

    return corners[index];*/

    /*uint mask = 1u << index;

    return float3(
        mix(aabb.min.x, aabb.max.x, float((mask & 1u) != 0)),
        mix(aabb.min.y, aabb.max.y, float((mask & 2u) != 0)),
        mix(aabb.min.z, aabb.max.z, float((mask & 4u) != 0))
    );*/
}

void AABBToSphere(AABB aabb, out float3 center, out float radius)
{
    //float greatest_extent = AABBGetGreatestExtent(aabb);
    center = AABBGetCenter(aabb);
    radius = length(AABBGetExtent(aabb)) * 0.5;  //greatest_extent * 0.5;
}

#endif