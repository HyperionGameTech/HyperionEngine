#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "./include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(View, GBufferTextures) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(View, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(View, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(View, GBufferMaterialTexture) uniform utexture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(View, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
HYP_DESCRIPTOR_SRV(View, GBufferWSNormalsTexture) uniform texture2D gbuffer_ws_normals_texture;
#endif

HYP_DESCRIPTOR_SRV(View, DeferredResult) uniform texture2D DeferredResult;

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(View, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

HYP_DESCRIPTOR_SRV(View, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(View, TAAResultTexture) uniform texture2D temporal_aa_result;
HYP_DESCRIPTOR_SRV(View, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(View, SSAOResultTexture) uniform texture2D ssao_gi;
HYP_DESCRIPTOR_SRV(View, DeferredIndirectResultTexture) uniform texture2D deferred_indirect_lighting;

HYP_DESCRIPTOR_CBUFF(View, PostProcessingUniforms) uniform PostProcessingUniforms
{
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
    uvec2 _pad;
}
post_processing;

#include "./include/shared.inc"
#include "./include/gbuffer.inc"
#include "./include/object.inc"
#include "./include/PostFXSample.inc"
#include "./include/tonemap.inc"
#include "./include/scene.inc"

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(View, PostFXPreStack, count = 4) uniform texture2D effects_pre_stack[4];
HYP_DESCRIPTOR_SRV(View, PostFXPostStack, count = 4) uniform texture2D effects_post_stack[4];
#endif

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
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
    const float peakNits = 1000.0; // @TODO: Make configurable
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif
}