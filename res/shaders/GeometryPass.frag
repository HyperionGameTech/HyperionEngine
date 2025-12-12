#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

#include "include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec2 v_texcoord1;
layout(location = 4) in vec3 v_tangent;
layout(location = 5) in vec3 v_bitangent;
layout(location = 7) in flat vec3 v_camera_position;
layout(location = 11) in vec4 v_position_ndc;
layout(location = 12) in vec4 v_previous_position_ndc;
layout(location = 15) in flat uint v_object_index;
layout(location = 16) in flat uint v_object_mask;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec4 gbuffer_material;
layout(location = 3) out vec2 gbuffer_velocity;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#define HAS_REFRACTION 1

#include "include/scene.inc"
#include "include/material.inc"
#include "include/Entity.glsl"
#include "include/packing.inc"

#include "include/env_probe.inc"
#include "include/gbuffer.inc"

HYP_DESCRIPTOR_SRV(View, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};
HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

#ifdef SHADING_TYPE_FORWARD
#include "include/brdf.inc"
#include "deferred/DeferredLighting.glsl"
#include "include/shadows.inc"
#endif

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#ifdef INSTANCING
HYP_DESCRIPTOR_SSBO(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
};
#else
HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entity;
};
#endif

// @TODO Refactor to use LightsBuffer instead

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentLight) readonly buffer CurrentLight
{
    Light light;
};

#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Entity, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL (materials[entity.material_index % HYP_MAX_MATERIALS])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Entity, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif
#endif

#if HAS_PARALLAX_MAP
#include "include/parallax.inc"
#endif

void main()
{
    mat3 tbn_matrix = mat3(normalize(v_tangent), normalize(v_bitangent), normalize(v_normal));

    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);
    const vec3 P = v_position.xyz;
    const vec3 V = normalize(camera.position.xyz - P);

    gbuffer_albedo = CURRENT_MATERIAL.albedo;

    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);
    const float alpha_threshold = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ALPHA_THRESHOLD);

    const float normal_map_intensity = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_NORMAL_MAP_INTENSITY);

    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;

#if HAS_PARALLAX_MAP
    vec3 tangent_view = transpose(tbn_matrix) * view_vector;
    vec2 parallax_texcoord = ParallaxMappedTexCoords(
        CURRENT_MATERIAL.parallax_height,
        texcoord,
        normalize(tangent_view));

    texcoord = parallax_texcoord;
#endif

#if HAS_ALBEDO_MAP
    vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, AlbedoMap, texcoord);

#ifdef ALPHA_DISCARD
    if (albedo_texture.a < alpha_threshold)
    {
        discard;
    }
#endif

    gbuffer_albedo *= albedo_texture;
#endif

    gbuffer_albedo.a = max(gbuffer_albedo.a, 0.005);

    vec4 normals_texture = vec4(0.0);

#if HAS_NORMAL_MAP
    normals_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, NormalMap, texcoord) * 2.0 - 1.0;

    N = normalize(tbn_matrix * normals_texture.xyz);
#endif

#if SHADING_TYPE_FORWARD
    {
        const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
        const vec3 F0 = CalculateF0(gbuffer_albedo.rgb, metalness);
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));

        vec3 L = light.position_intensity.xyz;
        L -= v_position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);
        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));

        const vec3 R = normalize(reflect(-V, N));

        // gbuffer_albedo.rgb *= NdotL;
        // gbuffer_albedo.rgb *= light.position_intensity.w;

        vec3 Ft = vec3(0.0);
        vec3 Fr = vec3(0.0);
        vec3 Fd = vec3(0.0);

        vec3 irradiance = vec3(0.0);
        vec4 ibl = vec4(0.0);
        vec4 reflections = vec4(0.0);

        { // irradiance
            const vec3 F = CalculateFresnelTerm(F0, roughness, NdotV);

            const vec3 dfg = CalculateDFG(F, roughness, NdotV);
            const vec3 E = CalculateE(F0, dfg);
            const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            uvec2 mipChainDimensions = textureSize(gbuffer_mip_chain, 0);

            Ft = CalculateRefraction(
                mipChainDimensions,
                P, N, V,
                texcoord,
                F0, E,
                transmission, roughness,
                vec4(0.0),
                gbuffer_albedo,
                vec3(ao));

            // #ifdef ENV_GRID_ENABLED
            // CalculateEnvGridIrradiance(P, N, irradiance);
            // #endif

            Fd = gbuffer_albedo.rgb * irradiance * (1.0 - E) * ao;

            ibl = CalculateReflectionProbe(
                current_env_probe,
                P, N, R,
                camera.position.xyz,
                roughness);

            ibl.a = saturate(ibl.a);

            // vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
            // specular_ao *= energy_compensation;

            vec3 reflection_combined = (ibl.rgb * (1.0 - reflections.a)) + (reflections.rgb);
            Fr = reflection_combined * E;
        }

        Ft *= transmission;
        Fd *= (1.0 - transmission);

        vec3 indirect_lighting = Ft + Fd + Fr;
        vec3 direct_lighting = vec3(0.0);

        { // direct lighting
            float shadow = 1.0;

            if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && bool(light.flags & LF_SHADOW))
            {
                shadow = GetShadow(light, P, texcoord, camera.dimensions.xy, NdotL);
            }

            vec3 light_color = light.color.rgb;

            const float D = CalculateDistributionTerm(roughness, NdotH);
            const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
            const vec3 F = CalculateFresnelTerm(F0, roughness, LdotH);

            const vec3 dfg = CalculateDFG(F, roughness, NdotV);
            const vec3 E = CalculateE(F0, dfg);
            const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            const vec3 diffuse_color = CalculateDiffuseColor(gbuffer_albedo.rgb, metalness);
            const vec3 specular_lobe = D * G * F;

            vec3 specular = specular_lobe;

            const vec2 radiusFalloff = unpackHalf2x16(light.radius_falloff);
            const float radius = radiusFalloff.x;
            const float falloff = radiusFalloff.y;

            const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
                ? GetSquareFalloffAttenuation(P, light.position_intensity.xyz, radius)
                : 1.0;

            vec3 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
            vec3 diffuse = diffuse_lobe;
            diffuse *= (1.0 - transmission);

            vec3 direct_component = diffuse + specular * energy_compensation;

            direct_lighting += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        }

        vec3 lighting = indirect_lighting + direct_lighting;

        // overwrite gbuffer_albedo with lit result
        gbuffer_albedo.rgb = lighting;
    }
#endif

#if HAS_METALNESS_MAP
    float metalness_sample = SAMPLE_TEXTURE(CURRENT_MATERIAL, MetalnessMap, texcoord).r;

    metalness = metalness_sample;
#endif

#if HAS_ROUGHNESS_MAP
    float roughness_sample = SAMPLE_TEXTURE(CURRENT_MATERIAL, RoughnessMap, texcoord).r;

    roughness = roughness_sample;
#endif

#if DEBUG_REFLECTIONS
    roughness = 0.01;
#endif

#if HAS_AO_MAP
    ao = SAMPLE_TEXTURE(CURRENT_MATERIAL, AoMap, texcoord).r;
#endif

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));

    uint mask = v_object_mask;

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = metalness;
    materialParams.mask = mask;

    gbuffer_normals = GBufferPackNormal(N);

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);
    
    gbuffer_normals.x = roughnessAndMetalPacked;
    gbuffer_material.x = maskPacked;
    gbuffer_material.y = 0u;
#ifdef SHADING_TYPE_LIGHTMAPPED
    gbuffer_material.z = floatBitsToUint(v_texcoord1.x);
    gbuffer_material.w = floatBitsToUint(v_texcoord1.y);
#else
    gbuffer_material.z = 0u;
    gbuffer_material.w = 0u;
#endif

    gbuffer_velocity = velocity;
}
