#include "../include/Defines.hlsli"

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

// @NOTE: Do not remove texture inputs even if unused,
// as we render in the same pass as DeferredDirect/DeferredIndirect and if they are not declare here,
// we will end up ending + restarting the pass so we can transition the image layouts correctly.
// In order to keep the same pass going without using LOAD operations and more complex management,
// we just keep the shader inputs the same as the other deferred shaders, even if some of them are not used.

DECLARE_SRV(LightmapPass, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(LightmapPass, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(LightmapPass, GBufferMaterialTexture) Texture2D<uint> gbuffer_material_texture;
DECLARE_SRV(LightmapPass, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;
DECLARE_SRV(LightmapPass, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

DECLARE_SRV(LightmapPass, GBufferMipChain) Texture2D gbuffer_mip_chain;

DECLARE_SAMPLER(LightmapPass, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(LightmapPass, SamplerLinear) SamplerState sampler_linear;

DECLARE_SRV(LightmapPass, SSAOResultTexture) Texture2D SSAOResultTexture;

#include "../include/Shared.hlsli"
#include "../include/Gbuffer.hlsli"
#include "../include/Entity.hlsli"
#include "../include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(LightmapPass, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(LightmapPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "../include/BRDF.hlsli"

DECLARE_SRV(LightmapPass, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(LightmapPass, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

// #include "../include/Shadows.hlsli"

DECLARE_SRV(LightmapPass, IrradianceTexture) Texture2D IrradianceTexture;
DECLARE_SRV(LightmapPass, RadianceTexture) Texture2D RadianceTexture;
DECLARE_SAMPLER(LightmapPass, LightmapSampler) SamplerState LightmapSampler;

DECLARE_BUFFER(LightmapPass, LightmapVolumeUniforms) cbuffer LightmapVolumeUniforms
{
    float irradianceWeight;
    float radianceWeight;

    uint numAtlases;
};

#include "../include/EnvProbes.hlsli"

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(LightmapPass, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
#else // !ENV_PROBE_CUBEMAP
DECLARE_SRV(LightmapPass, EnvProbesColorTexture) Texture2DArray envProbesColorTexture;
#endif // ENV_PROBE_CUBEMAP

DECLARE_SRV(LightmapPass, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

DECLARE_SRV_DYNAMIC(LightmapPass, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#include "./DeferredLighting.hlsli"

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    gbuffer_albedo_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    const float4 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_albedo_texture, texcoord, 0);
    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_normals_texture, texcoord, 0);

    const uint materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0));

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData >> 25u, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;

    const float perceptualRoughness = sqrt(roughness);

    float ao = 1.0;

    const float4x4 inverse_proj = camera.invProjMat;
    const float4x4 inverse_view = camera.invViewMat;

    float3 N = GBufferUnpackNormal(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_normals_texture, texcoord, 0));
    float2 UV1 = (float2((float)(materialData & 0xFFFu), (float)((materialData >> 12) & 0xFFFu)) + 0.5) / 4096.0;

    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_depth_texture, texcoord, 0).r;
    const float3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;
    const float3 V = normalize(camera.position.xyz - P);
    // const float3 R = normalize(reflect(-V, N));

    ao = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, SSAOResultTexture, texcoord, 0).r;

    float2 lightmapUV = UV1;

    float4 irradiance = SAMPLE_TEXTURE_2D_LOD(LightmapSampler, IrradianceTexture, lightmapUV, 0) * irradianceWeight;

    // float4 radiance = SAMPLE_TEXTURE_2D_LOD(LightmapSampler, RadianceTexture, lightmapUV, 0) * radianceWeight;
    // radiance.a = 1.0;

    const float3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);

    const float NdotV = max(0.0001, dot(N, V));
    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 F = CalculateFresnelTerm(F0, perceptualRoughness, NdotV);
    const float3 dfg = CalculateDFG(F, roughness, NdotV);
    const float3 E = CalculateE(F0, dfg);

    float3 diffuseIndirect = diffuse_color.rgb * irradiance.rgb * (1.0 - E) * ao;

    output.color_output.rgb = diffuseIndirect;
    output.color_output.a = 1.0;

    return output;
}

#endif // PIXEL_SHADER
