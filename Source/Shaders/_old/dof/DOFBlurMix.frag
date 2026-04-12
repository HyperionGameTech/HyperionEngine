#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;
layout(location = 0) out vec4 color_output;

#include "../include/Shared.hlsli"
#include "../include/Defines.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/Scene.hlsli"

layout(set = DepthOfField, binding = 78) uniform texture2D dof_blur_vert;

void main()
{
    const float dof_start = 1.35;
    const float dof_end = 15.0;

    const float depth = SampleGBuffer(gbuffer_depth_texture, v_texcoord).r;

#if 0
    // temp
    const vec3 focal_point = camera.position.xyz + camera.direction.xyz * 6.0;

    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), v_texcoord, depth).xyz;

    const float blur_amount = smoothstep(dof_start, dof_end, distance(P, focal_point));
    color_output = mix(SAMPLE_TEXTURE_2D(gbuffer_sampler, gbuffer_deferred_result, v_texcoord), SAMPLE_TEXTURE_2D(gbuffer_sampler, dof_blur_vert, v_texcoord), blur_amount);
#else

    color_output = vec4(1.0, 0.0, 0.0, 1.0);
#endif
}