#include "../include/defines.inc"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
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

#include "../include/defines.inc"
#include "../include/noise.inc"
#include "../include/BRDF.hlsli"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 out_color : SV_Target0;
};

DECLARE_SRV(RenderSSAO, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(RenderSSAO, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(RenderSSAO, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;
DECLARE_SRV(RenderSSAO, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(RenderSSAO, GBufferDepthTexture) Texture2D gbuffer_depth_texture;
DECLARE_SRV(RenderSSAO, DeferredResult) Texture2D gbuffer_deferred_result;

DECLARE_SAMPLER(RenderSSAO, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RenderSSAO, SamplerLinear) SamplerState sampler_linear;
DECLARE_SRV(RenderSSAO, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_BUFFER(RenderSSAO, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_BUFFER_DYNAMIC(RenderSSAO, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/gbuffer.inc"
#include "../include/BlueNoise.inc"
#include "../include/Temporal.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS


PSOutput PSMain(PSInput input)
{
    PSOutput output;
    output.out_color = (float4)1.0;

    const uint2 coord = uint2(input.position_cs.xy);
    const float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    gbuffer_material_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;
    
    const float4 normalSample = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_normals_texture, texcoord);

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;

    const float depth = SAMPLE_TEXTURE_2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;

    if (depth > 0.99999)
    {
        return output;
    }

    float3 N = GBufferUnpackNormal(normalSample);

    float3 P = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth).xyz;
    float3 V = normalize(float3(0.0, 0.0, 0.0) - P);
    float3 view_space_normal = normalize(mul(camera.view, float4(N, 0.0)).xyz);

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(view_space_normal, tangent, bitangent);

    const float2 texel_size = float2(1.0, 1.0) / float2(gbufferDimensions.xy);
    const float texel_size_max = max(texel_size.x, texel_size.y);

    float3 ray_origin;

#define NUM_SAMPLES 32

    float2 rnd = float2(
        SampleBlueNoise(int(coord.x), int(coord.y), int(world_shader_data.frame_counter % NUM_SAMPLES) * 2, NUM_SAMPLES * 2),
        SampleBlueNoise(int(coord.x), int(coord.y), int(world_shader_data.frame_counter % NUM_SAMPLES) * 2 + 1, NUM_SAMPLES * 2));


    return output;
}

#endif // PIXEL_SHADER
