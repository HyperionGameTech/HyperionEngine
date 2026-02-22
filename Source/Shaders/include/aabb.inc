#ifndef HYP_AABB_GLSL
#define HYP_AABB_GLSL

#ifdef LANG_GLSL
const vec3 aabb_corners[8] = {
    vec3(0.0, 0.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(0.0, 1.0, 1.0),
    vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 1.0)
};
#elif defined(LANG_HLSL)
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
#endif

struct AABB
{
    vec3 min;
    vec3 max;
};

bool AABBContainsPoint(AABB aabb, vec3 vec)
{
    return (
        vec.x >= aabb.min.x && vec.y >= aabb.min.y && vec.z >= aabb.min.z
        && vec.x <= aabb.max.x && vec.y <= aabb.max.y && vec.z <= aabb.max.z
    );
}

vec3 AABBGetCenter(AABB aabb)
{
    return (aabb.max + aabb.min) * 0.5;
}

vec3 AABBGetExtent(AABB aabb)
{
    return aabb.max - aabb.min;
}

float AABBGetGreatestExtent(AABB aabb)
{
    vec3 extent = AABBGetExtent(aabb);

    return max(extent.x, max(extent.y, extent.z));
}
// https://github.com/nvpro-samples/gl_occlusion_culling/blob/master/cull-common.h
vec3 AABBGetCorner(AABB aabb, int index)
{
    const vec3 extent = AABBGetExtent(aabb);

    return aabb.min + aabb_corners[index] * extent;

    /*const vec3 corners[8] = {
        aabb.min,
        aabb.min + vec3(extent.x, 0.f, 0.f),
        aabb.min + vec3(0.f, extent.y, 0.f),
        aabb.min + vec3(0.f, 0.f, extent.z),
        aabb.min + vec3(extent.xy, 0.f),
        aabb.min + vec3(0.f, extent.yz),
        aabb.min + vec3(extent.x, 0.f, extent.z),
        aabb.max
    };

    return corners[index];*/

    /*uint mask = 1u << index;

    return vec3(
        mix(aabb.min.x, aabb.max.x, float((mask & 1u) != 0)),
        mix(aabb.min.y, aabb.max.y, float((mask & 2u) != 0)),
        mix(aabb.min.z, aabb.max.z, float((mask & 4u) != 0))
    );*/
}

void AABBToSphere(AABB aabb, out vec3 center, out float radius)
{
    //float greatest_extent = AABBGetGreatestExtent(aabb);
    center = AABBGetCenter(aabb);
    radius = length(AABBGetExtent(aabb)) * 0.5;  //greatest_extent * 0.5;
}

#endif