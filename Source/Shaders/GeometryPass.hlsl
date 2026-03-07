#include "include/defines.inc"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint4 gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#define HAS_REFRACTION 1

#include "include/material.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/scene.inc"
#include "include/packing.inc"

#include "include/env_probe.inc"
#include "include/gbuffer.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER(Default, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

#ifdef SHADING_TYPE_FORWARD

#if ENV_PROBE_CUBEMAP
DECLARE_SRV(Default, EnvProbesTexture) TextureCubeArray envProbesTexture;
#else
DECLARE_SRV(Default, EnvProbesTexture) Texture2DArray envProbesTexture;
#endif

DECLARE_SRV(Default, EnvProbesBuffer) StructuredBuffer<EnvProbe> env_probes;

DECLARE_SRV(Default, GBufferMipChain) Texture2D gbuffer_mip_chain;

DECLARE_SRV(Default, ShadowMapsTextureArray) Texture2DArray shadow_maps;
DECLARE_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "include/brdf.inc"
#include "deferred/DeferredLighting.inc"
#include "include/Shadows.hlsli"
#endif

#ifdef SHADING_TYPE_FORWARD
DECLARE_SRV_DYNAMIC(Default, CurrentEnvProbe) StructuredBuffer<EnvProbe> current_env_probe_buffer;
#define current_env_probe current_env_probe_buffer[0]

DECLARE_SRV_DYNAMIC(Default, CurrentLight) StructuredBuffer<Light> current_light_buffer;
#define light current_light_buffer[0]

DECLARE_BUFFER_DYNAMIC(Default, ShadowMapCBuffer) cbuffer ShadowMapCBuffer
{
    ShadowMap shadowMap;
};

#endif

DECLARE_SRV_DYNAMIC(Default, MaterialsBuffer) StructuredBuffer<Material> materials_buffer;
#define material materials_buffer[0]

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

#if HAS_PARALLAX_MAP
#include "include/parallax.inc"
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    
    float3x3 tbn_matrix = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));

    float3 view_vector = normalize(input.camera_position - input.position);
    float3 N = normalize(input.normal);
    const float3 P = input.position.xyz;
    const float3 V = normalize(camera.position.xyz - P);

    output.gbuffer_albedo = CURRENT_MATERIAL.albedo;

    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);
    const float alpha_threshold = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ALPHA_THRESHOLD);

    const float normal_map_intensity = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_NORMAL_MAP_INTENSITY);

    float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;

#if HAS_PARALLAX_MAP
    float3 tangent_view = mul(tbn_matrix, view_vector); 
    float2 parallax_texcoord = ParallaxMappedTexCoords(
        CURRENT_MATERIAL.parallax_height,
        texcoord,
        normalize(tangent_view));

    texcoord = parallax_texcoord;
#endif

#if HAS_DIFFUSE_MAP
    float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, texcoord);

#ifdef ALPHA_DISCARD
    if (albedo_texture.a < alpha_threshold)
    {
        discard;
    }
#endif

    output.gbuffer_albedo *= albedo_texture;
#endif

    output.gbuffer_albedo.a = max(output.gbuffer_albedo.a, 0.005);

    float4 normals_texture = float4(0.0, 0.0, 0.0, 0.0);

#if HAS_NORMAL_MAP
    normals_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, NormalMap, texcoord) * 2.0 - 1.0;
    N = normalize(mul(normals_texture.xyz, tbn_matrix));
#endif

#if SHADING_TYPE_FORWARD
    {
        const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
        const float3 F0 = CalculateF0(output.gbuffer_albedo.rgb, metalness);
        float3 F90 = saturate(dot(F0, (50.0 * 0.33).xxx));

        float3 L = light.position_intensity.xyz;
        L -= input.position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const float3 H = normalize(L + V);
        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));

        const float3 R = normalize(reflect(-V, N));

        float3 Ft = (float3)0;
        float3 Fr = (float3)0;
        float3 Fd = (float3)0;

        float3 irradiance = (float3)0;
        float4 ibl = (float4)0;
        float4 reflections = (float4)0;

        { // irradiance
            const float3 F = CalculateFresnelTerm(F0, roughness, NdotV);

            const float3 dfg = CalculateDFG(F, roughness, NdotV);
            const float3 E = CalculateE(F0, dfg);
            const float3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            uint2 mipChainDimensions;
            gbuffer_mip_chain.GetDimensions(mipChainDimensions.x, mipChainDimensions.y);

            Ft = CalculateRefraction(
                mipChainDimensions,
                P, N, V,
                texcoord,
                F0, E,
                transmission, roughness,
                float4(0.0, 0.0, 0.0, 0.0),
                output.gbuffer_albedo,
                float3(ao, ao, ao));

            Fd = output.gbuffer_albedo.rgb * irradiance * (1.0 - E) * ao;

            ibl = CalculateReflectionProbe(
                current_env_probe,
                P, N, R,
                camera.position.xyz,
                roughness);

            ibl.a = saturate(ibl.a);

            float3 reflection_combined = (ibl.rgb * (1.0 - reflections.a)) + (reflections.rgb);
            Fr = reflection_combined * E;
        }

        Ft *= transmission;
        Fd *= (1.0 - transmission);

        float3 indirect_lighting = Ft + Fd + Fr;
        float3 direct_lighting = 0;

        float shadow = 1.0;

        { // direct lighting

            if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && bool(light.flags & LF_SHADOW_CASTER))
            {
                shadow = GetShadow(shadowMap, P, texcoord, camera.dimensions.xy, NdotL);
            }

            float3 light_color = light.color.rgb;

            const float D = CalculateDistributionTerm(roughness, NdotH);
            const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
            const float3 F = CalculateFresnelTerm(F0, roughness, LdotH);

            const float3 dfg = CalculateDFG(F, roughness, NdotV);
            const float3 E = CalculateE(F0, dfg);
            const float3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            const float3 diffuse_color = CalculateDiffuseColor(output.gbuffer_albedo.rgb, metalness);
            const float3 specular_lobe = D * G * F;

            float3 specular = specular_lobe;

            const float2 radiusFalloff = float2(f16tof32(light.radiusFalloffPacked), f16tof32(light.radiusFalloffPacked >> 16));
            const float radius = radiusFalloff.x;
            const float falloff = radiusFalloff.y;

            const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
                ? GetSquareFalloffAttenuation(P, light.position_intensity.xyz, radius)
                : 1.0;

            float3 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
            float3 diffuse = diffuse_lobe;
            diffuse *= (1.0 - transmission);

            float3 direct_component = diffuse + specular * energy_compensation;

            direct_lighting += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        }

        float3 lighting = indirect_lighting + direct_lighting;

        // overwrite gbuffer_albedo with lit result
        output.gbuffer_albedo.rgb = lighting;

        output.gbuffer_albedo.rgb = float3(shadow, shadow, shadow);
    }
#endif

#if HAS_METALNESS_MAP
    float metalness_sample = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, MetalnessMap, texcoord).r;

    metalness = metalness_sample;
#endif

#if HAS_ROUGHNESS_MAP
    float roughness_sample = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, RoughnessMap, texcoord).r;

    roughness = roughness_sample;
#endif

#if DEBUG_REFLECTIONS
    roughness = 0.01;
#endif

#if HAS_AO_MAP
    ao = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, AoMap, texcoord).r;
#endif

    // https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/
    // see: "Motion Vectors" section
    float2 velocity = float2(((input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5) - ((input.previous_position_ndc.xy / input.previous_position_ndc.w) * 0.5 + 0.5));

    uint mask = input.object_mask;

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = metalness;
    materialParams.mask = mask;

    output.gbuffer_normals = GBufferPackNormal(N);

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);
    
    output.gbuffer_normals.x = roughnessAndMetalPacked;
    output.gbuffer_material.x = maskPacked;
    output.gbuffer_material.y = 0u;
#ifdef SHADING_TYPE_LIGHTMAPPED
    output.gbuffer_material.z = asuint(input.texcoord1.x);
    output.gbuffer_material.w = asuint(input.texcoord1.y);
#else
    output.gbuffer_material.z = 0u;
    output.gbuffer_material.w = 0u;
#endif

    output.gbuffer_velocity = velocity;

    return output;
}
