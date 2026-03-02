
#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;

layout(location = 0) out vec4 combinedDepths;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/packing.inc"

#include "../include/scene.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(CombineShadowMaps, Src0) uniform texture2D src0;
DECLARE_SRV(CombineShadowMaps, Src1) uniform texture2D src1;
DECLARE_SAMPLER(CombineShadowMaps, SamplerNearest) uniform sampler sampler_nearest;

void main()
{
    vec4 color0 = SAMPLE_TEXTURE_2D(sampler_nearest, src0, v_texcoord);
    vec4 color1 = SAMPLE_TEXTURE_2D(sampler_nearest, src1, v_texcoord);

#ifdef VSM
    // VSM stores as 16 bit float in each texture
    vec2 moments = mix(color0.xy, color1.xy, bvec2(color0.r < color1.r));

    combinedDepths = vec4(moments, 0.0, 0.0);
#else
    float unpackedDepth0 = color0.r;
    float unpackedDepth1 = color1.r;

    float depth = min(unpackedDepth0, unpackedDepth1);

    combinedDepths = vec4(depth);
#endif
}
