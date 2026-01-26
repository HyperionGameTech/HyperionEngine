#ifndef HYP_SCENE_GLSL
#define HYP_SCENE_GLSL

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

struct Light
{
    // 0
    uint type;
    // 4
    uint material_index; // for area lights - ~0u == no material
    // 8
    uint radius_falloff; // packed as half
    // 12
    uint flags;

    // 16
    vec4 position_intensity; // position or direction
    // 32
    vec4 color;
    // 48
    vec4 normal;             // for area lights/spot lights - x,y,z = normal - w = layer index for shadow maps

    // 64
    mat4 shadow_matrix;
    // 128
    vec4 aabb_min;
    // 144
    vec4 aabb_max;
    // 160
    vec4 dimensions_scale; // xy = slice dimensions in pixels, zw = slice dimensions relative to the atlas dimensions
    // 176
    vec2 offset_uv;

    // 184
    vec2 area_size; // for area lights = area size, for spot lights = spot angles
    // 192
};

// Maps to LightFlags
#define LF_NONE 0x0
#define LF_SHADOW 0x1
#define LF_SHADOW_PCF 0x2
#define LF_SHADOW_CONTACT_HARDENING 0x4
#define LF_SHADOW_VSM 0x8
#define LF_SHADOW_FILTER_MASK (LF_SHADOW_PCF | LF_SHADOW_CONTACT_HARDENING | LF_SHADOW_VSM)
#define LF_DEFAULT (LF_SHADOW | LF_SHADOW_PCF)

#define LIGHT_LAYER_INDEX(light) (FLOAT_BITS_TO_UINT(light.normal.w))

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