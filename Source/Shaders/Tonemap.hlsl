#include "include/defines.inc"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE(0) float3 a_position : POSITION;
    HYP_ATTRIBUTE(1) float3 a_normal : NORMAL;
    HYP_ATTRIBUTE(2) float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;

    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(Tonemap, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(Tonemap, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(Tonemap, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(Tonemap, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(Tonemap, DeferredResult) Texture2D DeferredResult;

DECLARE_SRV(Tonemap, ShadowMapsTextureArray) Texture2DArray shadow_maps;

DECLARE_SRV(Tonemap, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(Tonemap, GBufferDepthTexture) Texture2D gbuffer_depth_texture;
DECLARE_SAMPLER(Tonemap, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(Tonemap, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(Tonemap, RTRadianceResultTexture) Texture2D rt_radiance_final;

DECLARE_SRV(Tonemap, SSGIResultTexture) Texture2D ssgi_result;
DECLARE_SRV(Tonemap, TAAResultTexture) Texture2D temporal_aa_result;
DECLARE_SRV(Tonemap, SSRResultTexture) Texture2D ssr_result;
DECLARE_SRV(Tonemap, SSAOResultTexture) Texture2D ssao_gi;

DECLARE_BUFFER(Tonemap, PostProcessingUniforms) cbuffer PostProcessingUniforms
{
    struct
    {
        uint2 effect_counts;
        uint2 last_enabled_indices;
        uint2 masks;
        uint2 _pad;
    } post_processing;
};

#include "include/shared.inc"
#include "include/gbuffer.inc"
#include "include/Entity.inc"
#include "include/PostFXSample.inc"
#include "include/tonemap.inc"
#include "include/scene.inc"

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
DECLARE_SRV(Tonemap, PostFXPreStack, count = 4) Texture2D effects_pre_stack[4];
DECLARE_SRV(Tonemap, PostFXPostStack, count = 4) Texture2D effects_post_stack[4];
#endif

DECLARE_BUFFER(Tonemap, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(Tonemap, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

    float4 shaded_result = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, DeferredResult, texcoord);

    float4 color_output = shaded_result;

    color_output = SampleLastEffectInChain(HYP_STAGE_POST, texcoord, color_output);
    color_output = float4(Tonemap(color_output.rgb), 1.0);

#ifdef OUTPUT_PQ_HDR
    const float peakNits = 1000.0;
    color_output.rgb = LinearToPQ(color_output.rgb, peakNits);
#endif

    output.color_output = color_output;

    return output;
}

#endif // PIXEL_SHADER
