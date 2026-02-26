#include "../include/defines.inc"
#include "../include/shared.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/noise.inc"

DECLARE_SAMPLER(ReflectionsPass, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(ReflectionsPass, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear

DECLARE_SRV(ReflectionsPass, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(ReflectionsPass, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(ReflectionsPass, GBufferMaterialTexture) Texture2D<uint2> gbuffer_material_texture;
DECLARE_SRV(ReflectionsPass, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;
DECLARE_SRV(ReflectionsPass, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

DECLARE_BUFFER_DYNAMIC(ReflectionsPass, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(ReflectionsPass, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_SRV(ReflectionsPass, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SRV(ReflectionsPass, GBufferMipChain) Texture2D gbuffer_mip_chain;

#define HYP_DEFERRED_NO_RT_RADIANCE

#include "../include/BlueNoise.inc"

#include "../include/env_probe.inc"
DECLARE_SRV_DYNAMIC(ReflectionsPass, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(ReflectionsPass, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(ReflectionsPass, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

DECLARE_SRV(ReflectionsPass, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

#include "./DeferredLighting.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define SAMPLE_COUNT 4

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
    float3 v_position : TEXCOORD0;
    float2 v_texcoord : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position_cs = position;
    output.v_position = position.xyz;
    output.v_texcoord = input.a_texcoord0;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 v_position : TEXCOORD0;
    float2 v_texcoord : TEXCOORD1;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.v_texcoord;

    uint2 gbufferDimensions;
    gbuffer_albedo_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_depth_texture, texcoord, 0.0).r;
    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_normals_texture, texcoord, 0.0);

    const float3 N = GBufferUnpackNormal(normalSample);
    const float3 P = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;
    const float3 V = normalize(camera.position.xyz - P);

    uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    float roughness = materialParams.roughness;
    float perceptualRoughness = ComputePerceptualRoughness(roughness, N);

    const float lod = perceptualRoughness * 7.0; // log2(128) = 7, where 128 is the maximum mip level of the environment map

    float4 ibl = float4(0.0, 0.0, 0.0, 0.0);

    const uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 rnd = Hammersley(uint(i), uint(SAMPLE_COUNT));

        float3 H = ImportanceSampleGGX(rnd, N, roughness);
        H = normalize(tangent * H.x + bitangent * H.y + N * H.z);

        const float3 dir = normalize(2.0 * dot(V, H) * H - V);

        float4 sample_ibl = float4(0.0, 0.0, 0.0, 0.0);

        ApplyReflectionProbe(
            current_env_probe.texture_index,
            current_env_probe.world_position.xyz,
            current_env_probe.aabb_min.xyz,
            current_env_probe.aabb_max.xyz,
            P,
            dir,
            lod,
            sample_ibl);

        ibl += sample_ibl;
    }

    output.color_output = ibl * (1.0 / float(SAMPLE_COUNT));

    return output;
}

#endif // PIXEL_SHADER
