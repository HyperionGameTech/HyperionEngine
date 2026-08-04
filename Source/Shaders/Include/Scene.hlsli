#ifndef HYP_SCENE
#define HYP_SCENE

#include "Defines.hlsli"

struct WorldShaderData
{
    float game_time;
    uint frame_counter;
    uint _pad0;
    uint _pad1;
};

struct Camera
{
    float4x4 view;
    float4x4 projection;

    float4x4 viewProjMat;

    float4x4 invViewMat;
    float4x4 invProjMat;

    float4x4 prevViewProjMat;

    uint4 dimensions;
    float4 position;
    float4 direction;
    float4 jitter;

    float near;
    float far;
    float fov;
    float _pad0;

    float4 _pad1;
    float4 _pad2;
    float4 _pad3;
};

struct ShadowMap
{
    float4x4 viewProjMat;
    float4x4 invProjMat;

    float4 aabbMin;         // w = offsetUV.x
    float4 aabbMax;         // w = offsetUV.y
    float4 dimensionsScale; // xy = slice dimensions in pixels, zw = slice dimensions relative to the atlas dimensions

    uint layerIndex;
    float splitDistance;
    float _pad0;
    float _pad1;
};

struct Light
{
    uint type;
    uint material_index;        // for area lights - ~0u == no material
    uint radiusFalloffPacked;   // packed as half
    uint flags;

    float4 position_intensity;  // position or direction
    float4 color;
    float4 normal;              // for area lights/spot lights - x,y,z = normal

    float2 area_size;           // for area lights = area size, for spot lights = spot angles
    float2 _pad0;

    float4 _pad1;
    float4 _pad2;
    float4 _pad3;
};

// Maps to LightFlags
#define LF_NONE 0x0

#define LF_SHADOW_CASTER 0x1

#define LF_SHADOW_CSM_SPLIT_0 0x20
#define LF_SHADOW_CSM_SPLIT_1 0x40
#define LF_SHADOW_CSM_SPLIT_2 0x80
#define LF_SHADOW_CSM_SPLIT_3 0x100
#define LF_SHADOW_CSM_SPLIT_MASK (LF_SHADOW_CSM_SPLIT_0 | LF_SHADOW_CSM_SPLIT_1 | LF_SHADOW_CSM_SPLIT_2 | LF_SHADOW_CSM_SPLIT_3)

#define LF_DEFAULT (LF_SHADOW_CASTER | LF_SHADOW_PCF)

struct LightmapVolume
{
    float4 aabb_max;
    float4 aabb_min;

    uint texture_index; // index of the lightmap texture in the lightmap atlas
    uint _pad0;
    uint _pad1;
    uint _pad2;

    float4 _pad3;
};

float3 CalculateLightDirection(Light light, in float3 world_position)
{
    float3 L = light.position_intensity.xyz;
    L -= world_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    return L;
}

#endif
