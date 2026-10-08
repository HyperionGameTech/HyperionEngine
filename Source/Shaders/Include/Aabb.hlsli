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

bool TriangleOverlapsBox(float3 p0, float3 p1, float3 p2, float3 normal, float3 center, float3 halfExtent)
{
    if (abs(dot(normal, center - p0)) > dot(abs(normal), halfExtent))
    {
        return false;
    }

    const float3 corners[3] = { p0, p1, p2 };

    [unroll]
    for (uint edgeIndex = 0; edgeIndex < 3; edgeIndex++)
    {
        const float3 a = corners[edgeIndex];
        const float3 b = corners[(edgeIndex + 1) % 3];
        const float3 opposite = corners[(edgeIndex + 2) % 3];

        float3 inward = cross(normal, b - a);
        inward *= dot(inward, opposite - a) < 0.0 ? -1.0 : 1.0;

        if (dot(inward, center - a) < -dot(abs(inward), halfExtent))
        {
            return false;
        }
    }

    return true;
}

#endif