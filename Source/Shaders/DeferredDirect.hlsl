#include "include/defines.inc"

PERMUTE(LIGHT_TYPE, CLUSTERED, DIRECTIONAL, POINT, SPOT, AREA_RECT);

STATIC(MAX_CLUSTERED_SHADOW_MAPS, 16);
STATIC(TILE_Z_BINS, 16);
STATIC(TILE_SIZE, 32);

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
    float4 output_color : SV_Target0;
};

DECLARE_SRV(DeferredPass, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(DeferredPass, GBufferMaterialTexture) Texture2D<uint4> gbuffer_material_texture;
DECLARE_SRV(DeferredPass, GBufferVelocityTexture) Texture2D gbuffer_velocity_texture;

DECLARE_SRV(DeferredPass, GBufferMipChain) Texture2D gbuffer_mip_chain;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

DECLARE_SAMPLER(DeferredPass, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(DeferredPass, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(DeferredPass, SamplerShadow) SamplerComparisonState SamplerShadow;

DECLARE_SRV(DeferredPass, SSAOResultTexture) Texture2D SSAOResultTexture;

#define HYP_SAMPLER_SHADOW SamplerShadow

#include "include/env_probe.inc"
#include "include/shared.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/Entity.inc"

#include "include/scene.inc"

DECLARE_BUFFER(DeferredPass, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#ifndef LIGHT_TYPE_CLUSTERED
DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    Camera camera;
    Light currentLight;
#ifdef LIGHT_TYPE_DIRECTIONAL
    ShadowMap shadowMap0;
    ShadowMap shadowMap1;
    ShadowMap shadowMap2;
    ShadowMap shadowMap3;
#else // !LIGHT_TYPE_DIRECTIONAL
    ShadowMap shadowMap;
#endif // LIGHT_TYPE_DIRECTIONAL
};
#endif // !LIGHT_TYPE_CLUSTERED

#ifdef LIGHT_TYPE_AREA_RECT

DECLARE_SRV_DYNAMIC(DeferredPass, CurrentMaterial) StructuredBuffer<Material> light_material_buffer;
#define lightMaterial light_material_buffer[0]

DECLARE_SRV(Material, DiffuseMap) Texture2D DiffuseMap;

DECLARE_SRV(DeferredPass, LTCMatrixTexture) Texture2D ltc_matrix_texture;
DECLARE_SRV(DeferredPass, LTCBRDFTexture) Texture2D ltc_brdf_texture;

DECLARE_SAMPLER(DeferredPass, LTCSampler) SamplerState ltc_sampler;

#endif // LIGHT_TYPE_AREA_RECT

DECLARE_SRV(DeferredPass, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(DeferredPass, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DEFERRED_NO_REFRACTION
#define HYP_DEFERRED_NO_ENV_PROBE
#define HYP_DEFERRED_NO_RT_RADIANCE

#include "./deferred/DeferredLighting.inc"

#undef HYP_DEFERRED_NO_RT_RADIANCE
#undef HYP_DEFERRED_NO_REFRACTION
#undef HYP_DEFERRED_NO_ENV_PROBE

#include "include/Shadows.hlsli"

#include "include/LightSampling.inc"

#ifdef LIGHT_TYPE_CLUSTERED
#include "deferred/Tiles.hlsli"

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    Camera camera;

    // shadow maps for the view are not per-tile as we have a limited number of shadow maps and shadow maps are per-view
    // so not directly accessible from the Light data.
    // Use LightToShadowMapIndex to indirectly index into this array based on the light index in the clustered data.
    ShadowMap shadowMaps[MAX_CLUSTERED_SHADOW_MAPS];
};

// Map light bound index to shadow map index in cbuffer (uint32 each)
DECLARE_SRV(DeferredPass, ShadowMapIndexBuffer) ByteAddressBuffer LightToShadowMapIndex;

uint GetShadowMapIndexForLight(uint lightIndex)
{
    return LightToShadowMapIndex.Load(lightIndex * sizeof(uint));
}

#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    gbuffer_albedo_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 pixelCoord = uint2(texcoord * max(0, int2(gbufferDimensions) - 1));

    float4 albedo = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture, texcoord, 0);
    float4 normalSample = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord, 0);
    float3 normal = GBufferUnpackNormal(normalSample);

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(normal, tangent, bitangent);

    const float depth = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord, 0).r;

    float4 positionVS = ReconstructViewSpacePositionFromDepth(camera.invProjMat, texcoord, depth);

    float4 position = mul(camera.invViewMat, positionVS);
    position /= position.w;

    uint2 materialData = gbuffer_material_texture.Load(int3(pixelCoord, 0)).xy;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;
    const float metalness = materialParams.metalness;
    const uint mask = materialParams.mask;

    float4 result = (float4)0;
    
    const float material_reflectance = 0.5;
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    float4 F0 = float4(albedo.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);

    const float4 diffuseColor = CalculateDiffuseColor(albedo, metalness);

    float4 F90 = (float4)saturate(dot(F0, (float4)(50.0 * 0.33)));

    float3 N = normalize(normal);
    float3 T = normalize(tangent);
    float3 B = normalize(bitangent);
    float3 V = normalize(camera.position.xyz - position.xyz);
    float3 H = (float3)0.0;

    const float NdotV = max(0.000001, dot(N, V));

    float ao = 1.0;
    float shadow = 1.0;

#if HBAO_ENABLED || SSAO_ENABLED
    const float4 ssao_data = SAMPLE_TEXTURE_2D_LOD(HYP_SAMPLER_NEAREST, SSAOResultTexture, texcoord, 0);
    ao = ssao_data.r;
#endif

    float4 area_light_radiance;
    
#ifdef LIGHT_TYPE_CLUSTERED
    // Cluster data
    const uint gridIndex = Cluster_GetGridIndex(
        gbufferDimensions, pixelCoord,
        positionVS.z / positionVS.w,
        camera.near, camera.far);

    const uint2 clusterData = ClusterGridBuffer[gridIndex];
    
    const uint clusterIndexOffset = clusterData.x;

    const uint numLights = (clusterData.y & 0xFFFF);
    const uint numEnvProbes = (clusterData.y >> 16) & 0xFFFF;

    // for testing
    bool lightHit = false;

    for (uint i = 0; i < numLights; ++i)
    {
        const uint lightIndex = Cluster_LoadLightIndex(clusterIndexOffset, i);
        
        // We handle area lights in a specialized version of the deferred pass, so we skip
        // them for clustered deferred shading.

        Light currentLight = LightsBuffer.Load(lightIndex);

        float3 L = currentLight.position_intensity.xyz;
        L -= position.xyz * float(min(currentLight.type, 1));

        L = normalize(L);

        H = normalize(L + V);

        const float NdotL = max(0.000001, dot(N, L));
        const float LdotH = max(0.000001, dot(L, H));
        const float NdotH = max(0.000001, dot(N, H));
        const float HdotV = max(0.000001, dot(H, V));

        float4 light_color = currentLight.color;

        float attenuation = 1.0;

        float shadow = 1.0; // @TODO shadows for clustered deferred

        const float D = CalculateDistributionTerm(roughness, NdotH);
        const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
        const float4 F = CalculateFresnelTerm(F0, roughness, LdotH);

        const float4 dfg = CalculateDFG(F, roughness, NdotV);
        const float4 E = CalculateE(F0, dfg);
        const float3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

        const float4 specular_lobe = D * G * F;

        switch (currentLight.type)
        {
            case HYP_LIGHT_TYPE_POINT:
            case HYP_LIGHT_TYPE_SPOT: // fallthrough
            {
                const float2 radiusFalloff = float2(f16tof32(currentLight.radiusFalloffPacked), f16tof32(currentLight.radiusFalloffPacked >> 16));
                const float radius = radiusFalloff.x;
                const float falloff = radiusFalloff.y;

                attenuation = GetSquareFalloffAttenuation(position.xyz, currentLight.position_intensity.xyz, radius);

                if (currentLight.type == HYP_LIGHT_TYPE_SPOT)
                {
                    float theta = max(dot(-L, normalize(currentLight.normal.xyz)), 0.0);
                    float2 spot_angles = currentLight.area_size.xy;

                    attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);
                    
                    // @TODO Spot shadows for clustered deferred
                    
                }
                else
                {
                    if ((currentLight.flags & LF_SHADOW_CASTER) != 0)
                    {
                        uint shadowMapIndex = GetShadowMapIndexForLight(lightIndex);
                        ShadowMap shadowMap = shadowMaps[shadowMapIndex];

                        float3 worldToLight = position.xyz - currentLight.position_intensity.xyz;

                        shadow = GetPointShadow(shadowMap, currentLight.flags, worldToLight, NdotL);
                    }
                }

                break;
            }
            default: break;
        }

        float4 specular = specular_lobe;

        float4 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;
        float4 diffuse = diffuse_lobe;

        float4 direct_component = diffuse + specular * float4(energy_compensation, 1.0);

        result += float4((direct_component * (light_color * ao * NdotL * shadow * currentLight.position_intensity.w * attenuation)).rgb, attenuation);

        lightHit = true;
    }

    // // Debug clustering
    // if (lightHit)
    // {
    //     result = float4(1.0, 0.0, 0.0, 1.0);
    // }
    // else
    // {
    //     result = float4(0.0, 0.0, 0.0, 0.0);
    // }

    // Env probes will be in indirect pass.
#else // !LIGHT_TYPE_CLUSTERED

#if defined(LIGHT_TYPE_DIRECTIONAL) || defined(LIGHT_TYPE_POINT) || defined(LIGHT_TYPE_SPOT)
    float3 L = currentLight.position_intensity.xyz;
    L -= position.xyz * float(min(currentLight.type, 1));
    L = normalize(L);

    H = normalize(L + V);

    const float NdotL = max(0.000001, dot(N, L));
    const float LdotH = max(0.000001, dot(L, H));
    const float NdotH = max(0.000001, dot(N, H));
    const float HdotV = max(0.000001, dot(H, V));
#elif defined(LIGHT_TYPE_AREA_RECT)
    float3 L;

    const float3 R = reflect(-V, N);

    float2 lut_uv = (float2(roughness, sqrt(1.0 - NdotV)));
    lut_uv.y = 1.0 - lut_uv.y;
    lut_uv = lut_uv * lut_scale + lut_bias;
    lut_uv = clamp(lut_uv, float2(0.0, 0.0), float2(1.0, 1.0));

    const float4 t1 = SAMPLE_TEXTURE_2D(ltc_sampler, ltc_matrix_texture, lut_uv);
    const float4 t2 = SAMPLE_TEXTURE_2D(ltc_sampler, ltc_brdf_texture, lut_uv);

    const float3x3 Minv = float3x3(
        float3(t1.x, 0.0, t1.y),
        float3(0.0, 1.0, 0.0),
        float3(t1.z, 0.0, t1.w));

    const float half_width = currentLight.area_size.x * 0.5;
    const float half_height = currentLight.area_size.y * 0.5;

    float3 light_tangent;
    float3 light_bitangent;
    ComputeOrthonormalBasis(currentLight.normal.xyz, light_tangent, light_bitangent);

    const float3 p0 = currentLight.position_intensity.xyz - light_tangent * half_width - light_bitangent * half_height;
    const float3 p1 = currentLight.position_intensity.xyz + light_tangent * half_width - light_bitangent * half_height;
    const float3 p2 = currentLight.position_intensity.xyz + light_tangent * half_width + light_bitangent * half_height;
    const float3 p3 = currentLight.position_intensity.xyz - light_tangent * half_width + light_bitangent * half_height;

    const float3 pts[4] = { p0, p1, p2, p3 };

    float4 area_light_diffuse = CalculateAreaLightRadiance(currentLight, (float3x3)1.0, pts, position.xyz, N, V);
    area_light_diffuse *= diffuseColor * (1.0 / HYP_FMATH_PI);

    float4 area_light_specular = CalculateAreaLightRadiance(currentLight, Minv, pts, position.xyz, N, V);

    area_light_specular *= diffuseColor * t2.x + (float4(1.0, 1.0, 1.0, 1.0) - diffuseColor) * t2.y;
    area_light_radiance = area_light_specular + area_light_diffuse;

    const float NdotL = 0.0;
    const float LdotH = 0.0;
    const float NdotH = 0.0;
    const float HdotV = 0.0;
#else // !LIGHT_TYPE_DIRECTIONAL && !LIGHT_TYPE_POINT && !LIGHT_TYPE_SPOT && !LIGHT_TYPE_AREA_RECT
    const float NdotL = 0.0;
    const float LdotH = 0.0;
    const float NdotH = 0.0;
    const float HdotV = 0.0;
#endif // LIGHT_TYPE_DIRECTIONAL || LIGHT_TYPE_POINT || LIGHT_TYPE_SPOT || LIGHT_TYPE_AREA_RECT

    float4 light_color = currentLight.color;

#ifdef LIGHT_TYPE_POINT
    if ((currentLight.flags & LF_SHADOW_CASTER) != 0)
    {
        const float3 worldToLight = position.xyz - currentLight.position_intensity.xyz;

        shadow = GetPointShadow(shadowMap, currentLight.flags, worldToLight, NdotL);
    }
#elif defined(LIGHT_TYPE_DIRECTIONAL)
    if ((currentLight.flags & LF_SHADOW_CASTER) != 0)
    {
        shadow = GetShadow(shadowMap0, currentLight.flags, position.xyz, texcoord, camera.dimensions.xy, NdotL);
    }
#endif // LIGHT_TYPE_POINT

    const float D = CalculateDistributionTerm(roughness, NdotH);
    const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
    const float4 F = CalculateFresnelTerm(F0, roughness, LdotH);

    const float4 dfg = CalculateDFG(F, roughness, NdotV);
    const float4 E = CalculateE(F0, dfg);
    const float3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

    const float4 specular_lobe = D * G * F;

#if defined(LIGHT_TYPE_POINT) || defined(LIGHT_TYPE_SPOT)
    const float2 radiusFalloff = float2(f16tof32(currentLight.radiusFalloffPacked), f16tof32(currentLight.radiusFalloffPacked >> 16));
    const float radius = radiusFalloff.x;
    const float falloff = radiusFalloff.y;

    float attenuation = GetSquareFalloffAttenuation(position.xyz, currentLight.position_intensity.xyz, radius);

#ifdef LIGHT_TYPE_SPOT
    float theta = max(dot(-L, normalize(currentLight.normal.xyz)), 0.0);
    float2 spot_angles = currentLight.area_size.xy;

    attenuation *= saturate((theta - spot_angles[0]) / (spot_angles[1] - spot_angles[0])) * step(spot_angles[0], theta);
#endif // LIGHT_TYPE_SPOT

#else // LIGHT_TYPE_POINT || LIGHT_TYPE_SPOT
    const float attenuation = 1.0;
#endif // LIGHT_TYPE_POINT || LIGHT_TYPE_SPOT

    float4 specular = specular_lobe;

    float4 diffuse_lobe = diffuseColor * HYP_FMATH_ONE_OVER_PI;
    float4 diffuse = diffuse_lobe;

    float4 direct_component = diffuse + specular * float4(energy_compensation, 1.0);

    result += direct_component * (light_color * ao * NdotL * shadow * currentLight.position_intensity.w * attenuation);
    result.a = attenuation;

#ifdef LIGHT_TYPE_AREA_RECT
    result = area_light_radiance;
#endif // LIGHT_TYPE_AREA_RECT

#endif // LIGHT_TYPE_CLUSTERED

#if defined(DEBUG_REFLECTIONS)      \
    || defined(DEBUG_IRRADIANCE)    \
    || defined(PATHTRACER)          \
    || defined(DEBUG_VELOCITY)      \
    || defined(DEBUG_NORMALS)       \
    || defined(DEBUG_AO)
    output.output_color = float4(0.0, 0.0, 0.0, 0.0);
#else
    output.output_color = float4(result);
#endif

    return output;
}

#endif // PIXEL_SHADER
