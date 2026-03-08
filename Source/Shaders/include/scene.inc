#ifndef HYP_SCENE
#define HYP_SCENE

#include "defines.inc"

struct WorldShaderData
{
    vec4 fog_params;

    float game_time;
    uint frame_counter;
    uint _pad0;
    uint _pad1;
};

struct Camera
{
    mat4 view;
    mat4 projection;

    mat4 viewProjMat;

    mat4 invViewMat;
    mat4 invProjMat;

    mat4 prevViewProjMat;

    uvec4 dimensions;
    vec4 position;
    vec4 direction;
    vec4 jitter;

    float near;
    float far;
    float fov;
    float _pad0;

    vec4 _pad1;
    vec4 _pad2;
    vec4 _pad3;
};

struct ShadowMap
{
    // layer indices per cascade for shadow maps. Each cascade is 1 byte.
    uint layerIndex;
    float splitDistance;
    float _pad0;
    float _pad1;
    
    float4x4 viewProjMat;
    float4 aabbMin;         // w = offsetUV.x
    float4 aabbMax;         // w = offsetUV.y
    float4 dimensionsScale; // xy = slice dimensions in pixels, zw = slice dimensions relative to the atlas dimensions
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
};

// Maps to LightFlags
#define LF_NONE 0x0

#define LF_SHADOW_CASTER 0x1
#define LF_SHADOW_PCF 0x2
#define LF_SHADOW_CONTACT_HARDENING 0x4
#define LF_SHADOW_VSM 0x8
#define LF_SHADOW_FILTER_MASK (LF_SHADOW_PCF | LF_SHADOW_CONTACT_HARDENING | LF_SHADOW_VSM)

#define LF_SHADOW_CSM_SPLIT_0 0x20
#define LF_SHADOW_CSM_SPLIT_1 0x40
#define LF_SHADOW_CSM_SPLIT_2 0x80
#define LF_SHADOW_CSM_SPLIT_3 0x100
#define LF_SHADOW_CSM_SPLIT_MASK (LF_SHADOW_CSM_SPLIT_0 | LF_SHADOW_CSM_SPLIT_1 | LF_SHADOW_CSM_SPLIT_2 | LF_SHADOW_CSM_SPLIT_3)

#define LF_DEFAULT (LF_SHADOW_CASTER | LF_SHADOW_PCF)

struct LightmapVolume
{
    vec4 aabb_max;
    vec4 aabb_min;

    uint texture_index; // index of the lightmap texture in the lightmap atlas
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

vec3 CalculateLightDirection(Light light, in vec3 world_position)
{
    vec3 L = light.position_intensity.xyz;
    L -= world_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    return L;
}

#endif