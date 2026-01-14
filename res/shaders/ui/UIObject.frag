#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_screen_space_position;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec4 v_color;
layout(location = 4) in flat uint v_object_index;
layout(location = 5) in flat uvec4 v_properties;

layout(location = 0) out vec4 gbuffer_albedo;

#define INSTANCING

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/material.inc"
#include "../include/Entity.glsl"
#include "../include/UIObject.glsl"
#include "../include/scene.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// clang-format off

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Default, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO(Default, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
};

HYP_DESCRIPTOR_SAMPLER(Default, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Default, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

HYP_DESCRIPTOR_SSBO_DYNAMIC(Default, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

#ifdef TEXTURED
HYP_DESCRIPTOR_SRV(Default, AlbedoMap) uniform texture2D AlbedoMap;
#endif

// clang-format on

float RoundedRectangle(vec2 pos, vec2 size, float radius)
{
    return 1.0 - clamp((length(max(abs(pos) - size + radius, 0.0)) - radius), 0.0, 1.0);
}

void main()
{
    const UIObjectProperties properties = GetUIObjectProperties(v_properties);

    vec4 ui_color = CURRENT_MATERIAL.albedo;

#ifdef TEXTURED
    vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, AlbedoMap, v_texcoord0);

    ui_color *= albedo_texture;
#endif

    // rounded corners
    vec2 size = vec2(properties.size);
    vec2 position = v_texcoord0 * size;

    if (!bool(HYP_FAST_LESS(properties.border_flags, 1)) && !bool(HYP_FAST_LESS(properties.border_radius, HYP_FMATH_EPSILON)))
    {
        float roundedness = RoundedRectangle((size * 0.5) - position, size * 0.5, properties.border_radius);

        float top = float((properties.border_flags & UOB_TOP) != 0u);
        float left = float((properties.border_flags & UOB_LEFT) != 0u);
        float bottom = float((properties.border_flags & UOB_BOTTOM) != 0u);
        float right = float((properties.border_flags & UOB_RIGHT) != 0u);

        roundedness = mix(mix(roundedness, 1.0, step(0.5, 1.0 - v_texcoord0.y)), roundedness, top);
        roundedness = mix(mix(roundedness, 1.0, step(0.5, 1.0 - v_texcoord0.x)), roundedness, left);
        roundedness = mix(mix(roundedness, 1.0, step(0.5, v_texcoord0.y)), roundedness, bottom);
        roundedness = mix(mix(roundedness, 1.0, step(0.5, v_texcoord0.x)), roundedness, right);

        ui_color.a *= mix(1.0, roundedness, 1.0 - step(properties.border_radius, 0.0));
    }

    gbuffer_albedo = ui_color * v_color;
}
