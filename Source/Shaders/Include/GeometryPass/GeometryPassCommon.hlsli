#ifndef HYP_GEOMETRY_PASS_COMMON_HLSLI
#define HYP_GEOMETRY_PASS_COMMON_HLSLI

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float4 color : TEXCOORD2;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
    nointerpolation uint cutout_seed : TEXCOORD8;
    nointerpolation float3 ground_normal : TEXCOORD9;
    nointerpolation float ground_normal_blend : TEXCOORD10;
    bool is_front_face : SV_IsFrontFace;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#define HAS_REFRACTION 1

#define MATERIAL_TEXTURE_MIP_BIAS (world_shader_data.taa_params.x)

#include "Include/Material.hlsli"

#include "Include/Scene.hlsli"
#include "Include/Packing.hlsli"
#include "Include/EnvProbes.hlsli"
#include "Include/Gbuffer.hlsli"
#include "Include/Entity.hlsli"
#include "Include/Noise.hlsli"
#include "Include/AlphaCutout.hlsli"
#include "Include/TerrainColor.hlsli"
#include "Include/TerrainMaterial.hlsli"

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Material, TerrainLayer0) Texture2D TerrainLayer0;
DECLARE_SRV(Material, TerrainLayer1) Texture2D TerrainLayer1;
DECLARE_SRV(Material, TerrainNormal0) Texture2D TerrainNormal0;
DECLARE_SRV(Material, TerrainNormal1) Texture2D TerrainNormal1;

#define DetailMap TerrainLayer0
#define CoverMap TerrainLayer1
#define DetailNormalMap TerrainNormal0
#define CoverNormalMap TerrainNormal1
#endif // !HYP_FEATURES_BINDLESS_TEXTURES

#define MESH_DETAIL_SCALE 0.2
#define MESH_DETAIL_FINE_SCALE 1.0
#define MESH_DETAIL_NORMAL_STRENGTH 1.0
#define MESH_DETAIL_FINE_NORMAL_STRENGTH 1.0
#define MESH_DETAIL_ALBEDO_STRENGTH 0.8
#define MESH_DETAIL_AO_STRENGTH 0.6
#define MESH_DETAIL_FADE float2(80.0, 250.0)
#define MESH_DETAIL_FINE_FADE float2(8.0, 35.0)

#define MESH_COVER_SLOPE float2(0.55, 0.85)
#define MESH_COVER_HEIGHT_BREAKUP 0.8

DECLARE_SRV(Default, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
    float4x4 vpMatrix;
};

#ifdef SHADING_TYPE_FORWARD

DECLARE_SRV(Default, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;
DECLARE_SRV(Default, EnvProbesDepthTexture) TextureCubeArray<float2> envProbesDepthTexture;

DECLARE_SRV(Default, GBufferMipChain) Texture2D GBufferMipChain;

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "Include/BRDF.hlsli"

#ifdef FORWARD_CLUSTERED

DECLARE_SRV(Default, EnvProbesBuffer) StructuredBuffer<EnvProbe> EnvProbesBuffer;
DECLARE_SRV(Default, LightsBuffer) StructuredBuffer<Light> LightsBuffer;
DECLARE_SRV(Default, ClusterGridBuffer) ByteAddressBuffer ClusterGridBuffer;
DECLARE_SRV(Default, ClusterIndexBuffer) ByteAddressBuffer ClusterIndexBuffer;

#include "Deferred/ClusteredShading.hlsli"

#include "Include/SkyVisibility.hlsli"

#include "VolumetricFog/VolumetricFogCommon.hlsli"

DECLARE_BUFFER_DYNAMIC(Default, ForwardIndirectConstants) cbuffer ForwardIndirectConstants
{
    EnvProbe skyProbe;
    SkyVisibilityCapture skyVisibilityCapture;
    VolumetricFogGrid volumetricFogGrid;
};

DECLARE_SRV(Default, SkyVisibilityTexture) Texture2D SkyVisibilityTexture;
DECLARE_SRV(Default, VolumetricFogTexture) Texture3D<float4> VolumetricFogTexture;

#ifdef RT_GI
DECLARE_SRV(Default, DDGIIrradianceTexture) Texture2D probe_irradiance;
DECLARE_SRV(Default, DDGIDepthTexture) Texture2D probe_depth;

#include "Include/RayTracing/GlobalIllumination/ProbeUniforms.hlsli"

DECLARE_BUFFER(Default, DDGIConstants) cbuffer DDGI
{
    DDGIConstants ddgiConstants;
};

#include "Include/RayTracing/GlobalIllumination/SampleDDGI.hlsli"
#endif // RT_GI

#define DEFERRED_LIGHTING_HAS_SKY
#endif // FORWARD_CLUSTERED

#include "Deferred/DeferredLighting.hlsli"

#undef DEFERRED_LIGHTING_HAS_SKY

#include "Include/Shadows.hlsli"

#ifdef FORWARD_SHADING

DECLARE_BUFFER_DYNAMIC(Default, ForwardShadingConstants) cbuffer ForwardShadingConstants
{
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
    EnvProbe fallbackProbe; // always the scene's sky probe, or a zeroed EnvProbe if none

    // CSM of the first directional light (matches DirectionalLightCSMData)
    float4x4 shadowViewMat;

    float4 atlasU;
    float4 atlasV;
    float4 atlasScaleX;
    float4 atlasScaleY;

    uint4 atlasSlice;

    float4 cascadeScaleX;
    float4 cascadeScaleY;
    float4 cascadeScaleZ;

    float4 cascadeOffsetX;
    float4 cascadeOffsetY;
    float4 cascadeOffsetZ;

    uint numBoundLights;
    uint directionalCSMLightIndex; // ~0u if none
};

// cascades follow the main camera, so a point outside all of them is treated as lit
float GetDirectionalCSMShadow(float3 position, float3 N, float NdotL)
{
    float4 positionLS = mul(shadowViewMat, float4(position, 1.0));
    positionLS /= positionLS.w;

    const float4 uvX = positionLS.x * cascadeScaleX + cascadeOffsetX;
    const float4 uvY = positionLS.y * cascadeScaleY + cascadeOffsetY;
    const float4 uvZ = positionLS.z * cascadeScaleZ + cascadeOffsetZ;

    const float4 maxDist = max(abs(uvX - 0.5), max(abs(uvY - 0.5), abs(uvZ - 0.5)));
    const float4 insideMask = step(maxDist, (float4)0.5);

    [branch]
    if (dot(insideMask, (float4)1.0) < 0.5)
    {
        return 1.0;
    }

    int cascadeIndex = 3;
    cascadeIndex = (insideMask.z > 0.5) ? 2 : cascadeIndex;
    cascadeIndex = (insideMask.y > 0.5) ? 1 : cascadeIndex;
    cascadeIndex = (insideMask.x > 0.5) ? 0 : cascadeIndex;

    const float cascadeWidth = 1.0 / max(abs(cascadeScaleX[cascadeIndex]), 0.000001);
    const float normalOffset = GetCascadeNormalOffset(cascadeWidth, NdotL);

    float4 offsetPositionLS = mul(shadowViewMat, float4(position + N * normalOffset, 1.0));
    offsetPositionLS /= offsetPositionLS.w;

    float4 shadowMapCoord;
    shadowMapCoord.x = offsetPositionLS.x * cascadeScaleX[cascadeIndex] + cascadeOffsetX[cascadeIndex];
    shadowMapCoord.y = offsetPositionLS.y * cascadeScaleY[cascadeIndex] + cascadeOffsetY[cascadeIndex];
    shadowMapCoord.z = offsetPositionLS.z * cascadeScaleZ[cascadeIndex] + cascadeOffsetZ[cascadeIndex];
    shadowMapCoord.w = (float)atlasSlice[cascadeIndex];

    return GetShadowCSM(shadowMapCoord,
        float2(atlasU[cascadeIndex], atlasV[cascadeIndex]),
        float2(atlasScaleX[cascadeIndex], atlasScaleY[cascadeIndex]));
}

#endif // FORWARD_SHADING

#endif // SHADING_TYPE_FORWARD

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif // CURRENT_MATERIAL

#include "Include/Parallax.hlsli"

#endif // HYP_GEOMETRY_PASS_COMMON_HLSLI
