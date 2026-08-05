#ifndef HYP_AABB
#define HYP_AABB

static const float3 s_aabbCorners[8] = {
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

    return aabb.min + s_aabbCorners[index] * extent;
}

float3 AABBGetCorner(float3 aabbMin, float3 aabbMax, int index)
{
    return aabbMin + s_aabbCorners[index] * (aabbMax - aabbMin);
}

void AABBToSphere(AABB aabb, out float3 center, out float radius)
{
    //float greatest_extent = AABBGetGreatestExtent(aabb);
    center = AABBGetCenter(aabb);
    radius = length(AABBGetExtent(aabb)) * 0.5;  //greatest_extent * 0.5;
}

#endif