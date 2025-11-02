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
    mat4 previous_view;

    uvec4 dimensions;
    vec4 position;
    vec4 direction;
    vec4 jitter;

    float near;
    float far;
    float fov;
    uint id;

    vec4 _pad1;
    vec4 _pad2;
    vec4 _pad3;

    mat4 _pad4;
    mat4 _pad5;
    mat4 _pad6;
};

struct Light
{
    uint type;
    uint material_index; // for area lights - ~0u == no material
    uint radius_falloff; // packed as half
    uint flags;

    vec2 area_size; // for area lights = area size, for spot lights = spot angles

    vec4 position_intensity; // position or direction
    vec4 color;

    vec4 normal;             // for area lights - x,y,z = normal

    /// shadow map data
    mat4 shadow_matrix;
    vec4 aabb_min;
    vec4 aabb_max;
    vec4 dimensions_scale; // xy = slice dimensions in pixels, zw = slice dimensions relative to the atlas dimensions
    vec2 offset_uv;
    uint layer_index; // atlas index, also used for point light shadow map index

};

// Maps to LightFlags
#define LF_NONE 0x0
#define LF_SHADOW 0x1
#define LF_SHADOW_PCF 0x2
#define LF_SHADOW_CONTACT_HARDENING 0x4
#define LF_SHADOW_VSM 0x8
#define LF_SHADOW_FILTER_MASK (LF_SHADOW_PCF | LF_SHADOW_CONTACT_HARDENING | LF_SHADOW_VSM)
#define LF_DEFAULT (LF_SHADOW | LF_SHADOW_PCF)

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