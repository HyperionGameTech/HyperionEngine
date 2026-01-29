
#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord0;

layout(location = 0) out vec4 out_color;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/shared.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(Global, FinalOutputTexture) uniform texture2D src_texture;
// DECLARE_SRV(Global, UITexture) uniform texture2D ui_texture;
DECLARE_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

void main()
{
    out_color = vec4(0.0, 0.0, 0.0, 1.0);

    out_color.rgb = SAMPLE_TEXTURE_2D(sampler_nearest, src_texture, v_texcoord0).rgb;

    // vec4 ui_color = SAMPLE_TEXTURE_2D(sampler_nearest, ui_texture, v_texcoord0);
    // out_color.rgb = mix(out_color.rgb, ui_color.rgb, ui_color.a);
}
