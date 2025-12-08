#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

#include "../include/defines.inc"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec4 v_positionNdc;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normals;
layout(location = 2) out uvec4 gbuffer_material;
layout(location = 3) out vec2 gbuffer_velocity;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler SamplerLinear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler SamplerNearest;

#define texture_sampler SamplerLinear

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(View, GBufferTextures) uniform texture2D GBufferTextures[NUM_GBUFFER_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(View, GBufferAlbedoTexture) uniform texture2D GBufferAlbedoTexture;
HYP_DESCRIPTOR_SRV(View, GBufferNormalsTexture) uniform texture2D GBufferNormalsTexture;
HYP_DESCRIPTOR_SRV(View, GBufferMaterialTexture) uniform utexture2D GBufferMaterialTexture;
HYP_DESCRIPTOR_SRV(View, GBufferVelocityTexture) uniform texture2D GBufferVelocityTexture;
#endif
HYP_DESCRIPTOR_SRV(View, GBufferDepthTexture) uniform texture2D GBufferDepthTexture;

#include "../include/scene.inc"
#include "../include/material.inc"
#include "../include/Entity.glsl"
#include "../include/packing.inc"
#include "../include/shared.inc"

#include "../include/env_probe.inc"
#include "../include/gbuffer.inc"

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
#include "../include/brdf.inc"
#include "DeferredLighting.glsl"
#include "../include/shadows.inc"
#endif

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entity;
};

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

#include "FogVolume.inl"

HYP_DESCRIPTOR_SRV(FogVolume, DataMap) uniform texture3D DataMap;
HYP_DESCRIPTOR_SRV(FogVolume, NoiseMap) uniform texture3D NoiseMap;
HYP_DESCRIPTOR_CBUFF(FogVolume, FogVolumeUniforms) uniform FogVolumeUniforms
{
    FogVolume fogVolume;
};

vec2 RayBoxIntersect(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax)
{
    vec3 invDir = 1.0 / rayDir;
    vec3 tMin = (boxMin - rayOrigin) * invDir;
    vec3 tMax = (boxMax - rayOrigin) * invDir;

    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);

    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    return vec2(tNear, tFar);
}

vec3 WorldToLocal(vec3 positionWS, in vec3 aabbMin, in vec3 aabbMax)
{
    return (positionWS - aabbMin) / (aabbMax - aabbMin);
}

vec3 LocalToTexCoord(vec3 positionLS)
{
    return clamp(positionLS, vec3(0.0), vec3(1.0));
}

vec3 WorldToTexCoord(vec3 positionWS, in vec3 aabbMin, in vec3 aabbMax)
{
    return clamp(LocalToTexCoord(WorldToLocal(positionWS, aabbMin, aabbMax)), vec3(0.0), vec3(1.0));
}

vec4 RayMarch(vec3 rayOrigin, vec3 rayDir, float tNear, float tFar, float stepSize)
{
    float t = tNear;
    const int maxSteps = 128;
    
    float transmittance = 1.0;
    vec3 accumulatedColor = vec3(0.0);

    const float DensityScale = 0.65; // Global density multiplier
    const float Scattering = 0.3 * DensityScale; // Scattering Coefficient
    const float Absorption = 0.2 * DensityScale; // Absorption Coefficient
    const float Extinction = Scattering + Absorption;    // Extinction Coefficient (Total light loss)
    
    const vec3 AmbientLight = vec3(0.7, 0.7, 0.7); // Light Color

    vec3 albedo = (Extinction > 1e-6) ? AmbientLight * (Scattering / Extinction) : vec3(0.0);

    for (int i = 0; i < maxSteps; i++)
    {
        if (t >= tFar || transmittance < 0.001)
        {
            break;
        }

        vec3 currentPos = rayOrigin + rayDir * t;
        vec3 uvw = WorldToTexCoord(currentPos, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);

        // SDF < 0 = Inside solid surface
        // SDF > 0 = Air (Fog)
        float sdf = Texture3D(texture_sampler, DataMap, uvw).r;

        // inside a surface, stop marching
        if (sdf < 0.0)
        {
            break; 
        }

        float noise = Texture3D(texture_sampler, NoiseMap, clamp(uvw, vec3(0.0), vec3(1.0))).r;
        float localDensity = DensityScale * noise;

        float currentExtinction = Extinction * localDensity;
        
        float stepTransmittance = exp(-currentExtinction * stepSize);
        vec3 stepScattering = albedo * (1.0 - stepTransmittance);

        accumulatedColor += transmittance * stepScattering;
        transmittance *= stepTransmittance;

        t += stepSize;
    }

    float alpha = 1.0 - transmittance;
    return vec4(max(vec3(0.0), accumulatedColor), alpha);
}

void main()
{
    vec2 screenSpaceUV = (v_positionNdc.xy / v_positionNdc.w) * 0.5 + 0.5;
    float sceneDepth = Texture2D(SamplerNearest, GBufferDepthTexture, screenSpaceUV).r;
    vec4 positionVS = ReconstructViewSpacePositionFromDepth(inverse(camera.projection), screenSpaceUV, sceneDepth);
    float linearDepth = positionVS.z / positionVS.w;

    vec4 positionWS = inverse(camera.view) * positionVS;
    positionWS /= positionWS.w;

    vec3 rayDir = normalize(positionWS.xyz - camera.position.xyz);

    vec2 boxHits = RayBoxIntersect(camera.position.xyz, rayDir, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);
    float tNear = boxHits.x;
    float tFar = boxHits.y;

    // Intersection tests
    if (tFar < 0.0 || tNear > tFar)
    {
        discard;
    }

    tFar = min(tFar, linearDepth);
    tNear = max(tNear, 0.0);

    if (tNear >= tFar)
    {
        discard;
    }
    
    vec4 fogColor = RayMarch(camera.position.xyz, rayDir, tNear, tFar, 0.1);

    // Debug: render the noise texture
    //vec3 uvw = WorldToTexCoord(positionWS.xyz, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);
    //fogColor = Texture3D(texture_sampler, NoiseMap, uvw);

    gbuffer_albedo = fogColor;
    gbuffer_normals = vec4(0.0);
    gbuffer_material = uvec4(0);
    gbuffer_velocity = vec2(0.0);
}