#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "./include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(Tonemap, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(Tonemap, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(Tonemap, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(Tonemap, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;

HYP_DESCRIPTOR_SRV(Tonemap, DeferredResult) uniform texture2D DeferredResult;

HYP_DESCRIPTOR_SRV(Tonemap, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;

HYP_DESCRIPTOR_SRV(Tonemap, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Tonemap, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Tonemap, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Tonemap, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Tonemap, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

HYP_DESCRIPTOR_SRV(Tonemap, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(Tonemap, TAAResultTexture) uniform texture2D temporal_aa_result;
HYP_DESCRIPTOR_SRV(Tonemap, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(Tonemap, SSAOResultTexture) uniform texture2D ssao_gi;

HYP_DESCRIPTOR_CBUFF(Tonemap, PostProcessingUniforms) uniform PostProcessingUniforms
{
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
    uvec2 _pad;
}
post_processing;

#include "./include/shared.inc"
#include "./include/gbuffer.inc"
#include "./include/Entity.glsl"
#include "./include/PostFXSample.inc"
#include "./include/tonemap.inc"
#include "./include/scene.inc"

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(Tonemap, PostFXPreStack, count = 4) uniform texture2D effects_pre_stack[4];
HYP_DESCRIPTOR_SRV(Tonemap, PostFXPostStack, count = 4) uniform texture2D effects_post_stack[4];
#endif

HYP_DESCRIPTOR_CBUFF(Tonemap, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Tonemap, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    // Shaded result with forward rendered objects included
    vec4 shaded_result = Texture2D(HYP_SAMPLER_NEAREST, DeferredResult, texcoord);

    color_output = shaded_result;

    color_output = SampleLastEffectInChain(HYP_STAGE_POST, texcoord, color_output);
    color_output = vec4(Tonemap(color_output.rgb), 1.0);

#ifdef OUTPUT_PQ_HDR
    const float peakNits = 1000.0; /// \todo : Make configurable
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif
}