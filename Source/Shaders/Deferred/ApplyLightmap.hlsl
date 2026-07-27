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
    float4 positionNdc : TEXCOORD0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#include "../include/Shared.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV_DYNAMIC(LightmapPass, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_BUFFER(LightmapPass, LightmapVolumeUniforms) cbuffer LightmapVolumeUniforms
{
    float4x4 transformMatrix;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 worldPosition = mul(transformMatrix, float4(input.a_position, 1.0));
    output.position = worldPosition.xyz / worldPosition.w;
    
    float4x4 jitterMat = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    jitterMat[0][3] += camera.jitter.x;
    jitterMat[1][3] += camera.jitter.y;

    output.positionNdc = mul(jitterMat, mul(camera.viewProjMat, float4(output.position, 1.0)));
    output.position_cs = output.positionNdc;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float4 positionNdc : TEXCOORD0;
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

DECLARE_SRV(LightmapPass, GBufferAlbedoTexture) Texture2D GBufferAlbedoTexture;
DECLARE_SRV(LightmapPass, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(LightmapPass, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(LightmapPass, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;
DECLARE_SRV(LightmapPass, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_SRV(LightmapPass, GBufferMipChain) Texture2D GBufferMipChain;

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
DECLARE_SAMPLER(LightmapPass, LightmapSampler) SamplerState LightmapSampler;

DECLARE_BUFFER(LightmapPass, LightmapVolumeUniforms) cbuffer LightmapVolumeUniforms
{
    float4x4 transformMatrix; // unused here, read by the vertex shader
    float4 aabbMin;           // volume bounds in world space
    float4 aabbMax;
    float irradianceWeight;
    uint numAtlases;
};

#include "../include/EnvProbes.hlsli"

DECLARE_SRV(LightmapPass, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

DECLARE_SRV(LightmapPass, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

DECLARE_SRV_DYNAMIC(LightmapPass, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

#include "./DeferredLighting.hlsli"

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = (input.positionNdc.xy / input.positionNdc.w) * 0.5 + 0.5;
    texcoord.y = 1.0 - texcoord.y;

    const float4x4 inverse_proj = camera.invProjMat;
    const float4x4 inverse_view = camera.invViewMat;

    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferDepthTexture, texcoord, 0).r;
    const float3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;

    // Rasterizing the volume's bounding cube only masks its screen-space silhouette -- geometry
    // well in front of or behind the volume still projects into it, and depth testing is off.
    // Reject any pixel whose shaded surface does not actually lie inside the volume, otherwise
    // unrelated geometry seen "through" the box picks up this volume's lightmap.
    const float3 distanceOutside = max(aabbMin.xyz - P, P - aabbMax.xyz);

    if (max(distanceOutside.x, max(distanceOutside.y, distanceOutside.z)) > 0.0)
    {
        discard;
    }

    uint2 gbufferDimensions;
    GBufferAlbedoTexture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(clamp(texcoord * int2(gbufferDimensions), (int2)0, int2(gbufferDimensions) - 1));

    const float4 albedo = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferAlbedoTexture, texcoord, 0);
    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferNormalsTexture, texcoord, 0);

    const uint materialData = GBufferMaterialTexture.Load(int3(pixelCoord, 0));

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData >> 28u, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;

    const float perceptualRoughness = sqrt(roughness);

    float ao = 1.0;

    float3 N = GBufferUnpackNormal(SAMPLE_TEXTURE_2D_LOD(sampler_nearest, GBufferNormalsTexture, texcoord, 0));
    float2 UV1 = (float2((float)(materialData & 0x3FFFu), 1.0 - (float)((materialData >> 14) & 0x3FFFu)) + 0.5) / 16384.0;

    const float3 V = normalize(camera.position.xyz - P);
    // const float3 R = normalize(reflect(-V, N));

    ao = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, SSAOResultTexture, texcoord, 0).r;

    float2 lightmapUV = UV1;

    const float4 irradiance = SAMPLE_TEXTURE_2D_LOD(LightmapSampler, IrradianceTexture, lightmapUV, 0) * irradianceWeight;

    const float3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);

    const float NdotV = max(0.0001, dot(N, V));
    const float3 F0 = CalculateF0(albedo.rgb, metalness);
    const float3 dfg = CalculateDFG(perceptualRoughness, NdotV);
    const float3 E = CalculateE(F0, dfg);

    float3 diffuseIndirect = diffuse_color.rgb * irradiance.rgb * (1.0 - E) * ao;

    output.color_output.rgb = diffuseIndirect;
    output.color_output.a = 1.0;

    return output;
}

#endif // PIXEL_SHADER
