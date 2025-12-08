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

HYP_DESCRIPTOR_SRV(FogVolume, VolumeTexture) uniform texture3D VolumeTexture;
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
    const vec3 extent = aabbMax - aabbMin;
    const vec3 center = aabbMin + extent * 0.5;

    return (positionWS - center) / extent;
}

vec3 LocalToTexCoord(vec3 positionLS)
{
    return clamp(positionLS + 0.5, vec3(0.0), vec3(1.0));
}

vec3 WorldToTexCoord(vec3 positionWS, in vec3 aabbMin, in vec3 aabbMax)
{
    return LocalToTexCoord(WorldToLocal(positionWS, aabbMin, aabbMax));
}

vec4 RayMarch(vec3 rayOrigin, vec3 rayDir, float tNear, float tFar, float stepSize)
{
    float t = tNear;
    const int maxSteps = 128;
    int steps = 0;

    float transmittance = 1.0;
    vec3 accumulatedColor = vec3(0.0);

    // Fallback constants if not defined in FogVolume
    // Original uniform-backed assignments (commented out while placeholders are used)
    // float density = fogVolume.density; // expected in uniforms
    float density = 0.7; // placeholder: replace with fogVolume.density
    // float absorption = fogVolume.absorption; // expected extinction coefficient
    float absorption = 0.1; // placeholder extinction coefficient
    // float scatter = fogVolume.scatter; // scattering coefficient
    float scatter = 0.1; // placeholder scattering coefficient
    // vec3 fogCol = fogVolume.color.rgb;
    vec3 fogCol = vec3(0.7, 0.7, 0.7); // placeholder fog color
    const float minStep = stepSize; // base step if SDF returns tiny values
    float jitter = float(world_shader_data.frame_counter & 1) * 0.5; // simple temporal jitter

    // Small blue noise/jitter to avoid banding
    t += (fract(sin(dot(rayOrigin.xy, vec2(12.9898,78.233))) * 43758.5453) - 0.5) * minStep * jitter;

    while (t < tFar && steps < maxSteps && transmittance > 1e-3)
    {
        vec3 posWS = rayOrigin + rayDir * t;
        vec3 uvw = WorldToTexCoord(posWS, fogVolume.aabbMin.xyz, fogVolume.aabbMax.xyz);

        // Sample signed distance field (r channel)
        float sdf = Texture3D(texture_sampler, VolumeTexture, uvw).r;

        // Sphere-tracing step respects SDF sign:
        //  - sdf > 0: outside, advance by positive distance to surface
        //  - sdf < 0: inside, advance by magnitude to exit while accumulating
        float stepDist = (sdf >= 0.0) ? max(sdf, minStep) : max(-sdf, minStep);

        // Accumulate fog only when in air (outside solid geometry): sdf >= 0
        if (sdf >= 0.0)
        {
            // thickness approximated by how far we move this iteration
            float ds = stepDist;

            // Fade out fog close to surfaces: lower SDF -> lower contribution
            // Placeholder fade distance; replace with uniform if available
            const float wispDistance = 0.2;
            float wisp = smoothstep(0.0, wispDistance, max(sdf, 0.0));
            float effectiveDensity = density * wisp;

            // Beer-Lambert attenuation
            float extinction = max(absorption + scatter, 1e-6);
            float attenuation = exp(-extinction * effectiveDensity * ds);

            // Single-scattering approximation: in-scattered light proportional to scatter
            vec3 inScatter = fogCol * (scatter * effectiveDensity) * (1.0 - attenuation);

            // Accumulate with current transmittance
            accumulatedColor += transmittance * inScatter;
            transmittance *= attenuation;
        }

        t += stepDist;
        steps++;
    }

    float alpha = clamp(1.0 - transmittance, 0.0, 1.0);
    return vec4(accumulatedColor, alpha);
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

    gbuffer_albedo = fogColor;
    gbuffer_normals = vec4(0.0);
    gbuffer_material = uvec4(0);
    gbuffer_velocity = vec2(0.0);
}