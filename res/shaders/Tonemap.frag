#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "./include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(Tonemap, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
DECLARE_SRV(Tonemap, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
DECLARE_SRV(Tonemap, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
DECLARE_SRV(Tonemap, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;

DECLARE_SRV(Tonemap, DeferredResult) uniform texture2D DeferredResult;

DECLARE_SRV(Tonemap, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;

DECLARE_SRV(Tonemap, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
DECLARE_SRV(Tonemap, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
DECLARE_SAMPLER(Tonemap, SamplerNearest) uniform sampler sampler_nearest;
DECLARE_SAMPLER(Tonemap, SamplerLinear) uniform sampler sampler_linear;

DECLARE_SRV(Tonemap, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

DECLARE_SRV(Tonemap, SSGIResultTexture) uniform texture2D ssgi_result;
DECLARE_SRV(Tonemap, TAAResultTexture) uniform texture2D temporal_aa_result;
DECLARE_SRV(Tonemap, SSRResultTexture) uniform texture2D ssr_result;
DECLARE_SRV(Tonemap, SSAOResultTexture) uniform texture2D ssao_gi;

DECLARE_BUFFER(Tonemap, PostProcessingUniforms) uniform PostProcessingUniforms
{
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
    uvec2 _pad;
}
post_processing;

#include "./include/shared.inc"
#include "./include/gbuffer.inc"
#include "./include/Entity.inc"
#include "./include/PostFXSample.inc"
#include "./include/tonemap.inc"
#include "./include/scene.inc"

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
DECLARE_SRV(Tonemap, PostFXPreStack, count = 4) uniform texture2D effects_pre_stack[4];
DECLARE_SRV(Tonemap, PostFXPostStack, count = 4) uniform texture2D effects_post_stack[4];
#endif

DECLARE_BUFFER(Tonemap, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(Tonemap, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    // Shaded result with forward rendered objects included
    vec4 shaded_result = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, DeferredResult, texcoord);

    color_output = shaded_result;

    color_output = SampleLastEffectInChain(HYP_STAGE_POST, texcoord, color_output);
    color_output = vec4(Tonemap(color_output.rgb), 1.0);

#ifdef OUTPUT_PQ_HDR
    const float peakNits = 1000.0; /// \todo : Make configurable
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif
}