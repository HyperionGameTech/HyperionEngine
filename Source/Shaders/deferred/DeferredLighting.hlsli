#ifndef DEFERRED_LIGHTING_HLSLI
#define DEFERRED_LIGHTING_HLSLI

#include "../include/shared.inc"
#include "../include/BRDF.hlsli"
#include "../include/Octahedron.inc"

struct Refraction
{
    float3 position;
    float3 direction;
};

void RefractionSolidSphere(
    float3 P, float3 N, float3 V, float eta_ir,
    out Refraction out_refraction)
{
    const float thickness = 0.8;

    const float3 R = refract(-V, N, eta_ir);
    float NdotR = dot(N, R);
    float d = thickness * -NdotR;
    float3 n1 = normalize(NdotR * R - N * 0.5);

    Refraction refraction;
    refraction.position = float3(P + R * d);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}

#ifndef HYP_DEFERRED_NO_REFRACTION

// Compute attenuated light as it travels through a volume.
float3 ApplyVolumeAttenuation(float3 radiance, float transmission_distance, float3 attenuation_color, float attenuation_distance)
{
    if (attenuation_distance == 0.0)
    {
        // Attenuation distance is +∞ (which we indicate by zero), i.e. the transmitted color is not attenuated at all.
        return radiance;
    }
    else
    {
        // Compute light attenuation using Beer's law.
        float3 attenuation_coefficient = -log(attenuation_color) / attenuation_distance;
        float3 transmittance = exp(-attenuation_coefficient * transmission_distance); // Beer's law
        return transmittance * radiance;
    }
}

float3 CalculateRefraction(
    uint2 image_dimensions,
    float3 P, float3 N, float3 V, float2 texcoord,
    float3 F0, float3 E,
    float transmission, float roughness,
    float4 opaque_color, float4 translucent_color,
    float3 brdf)
{
    // dimensions of mip chain image
    const uint max_dimension = max(image_dimensions.x, image_dimensions.y);

    const float IOR = 1.5;

    const float sqrt_F0 = sqrt(F0.y);
    const float material_ior = (1.0 + sqrt_F0) / (1.0 - sqrt_F0);
    const float air_ior = 1.0;
    const float eta_ir = air_ior / material_ior;
    const float eta_ri = material_ior / air_ior;

    Refraction refraction;
    RefractionSolidSphere(P, N, V, eta_ir, refraction);

    float4 refraction_pos = mul(camera.viewProjMat, float4(refraction.position, 1.0));

    refraction_pos /= refraction_pos.w;

    float2 refraction_texcoord = refraction_pos.xy * 0.5 + 0.5;

    const float lod = ApplyIORToRoughness(IOR, roughness) * log2(float(max_dimension));

    float absorption = 0.1; // TODO: material parameter
    float3 T = min(float3(1.0, 1.0, 1.0), exp(-absorption * refraction.direction));

    float3 Ft = SAMPLE_TEXTURE_2D_LOD(sampler_linear, gbuffer_mip_chain, refraction_texcoord, lod).rgb;
    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E;
    Ft *= T;

    return Ft;
}

#endif // HYP_DEFERRED_NO_REFRACTION

#ifndef HYP_DEFERRED_NO_ENV_PROBE

#include "../include/env_probe.inc"

void ApplyReflectionProbe(uint probe_texture_index, float3 probe_world_position, float3 aabb_min, float3 aabb_max, float3 P, float3 R, float lod, inout float4 ibl)
{
    ibl = float4(0.0, 0.0, 0.0, 0.0);

    probe_texture_index = min(probe_texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1);

    const float3 extent = (aabb_max - aabb_min);
    const float3 center = (aabb_max + aabb_min) * 0.5;
    const float3 diff = P - center;

#ifdef ENV_PROBE_PARALLAX_CORRECTED
    R = EnvProbeCoordParallaxCorrected(probe_world_position, aabb_min, aabb_max, P, R);
#endif // ENV_PROBE_PARALLAX_CORRECTED

#if ENV_PROBE_CUBEMAP
    ibl = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(sampler_linear, envProbesTexture, float4(normalize(R), float(probe_texture_index)), lod);
#else // !ENV_PROBE_CUBEMAP
    ibl = SAMPLE_TEXTURE_2D_ARRAY_LOD(sampler_linear, envProbesTexture, float3(EncodeOctahedralCoord(normalize(R)) * 0.5 + 0.5, float(probe_texture_index)), lod);
#endif // ENV_PROBE_CUBEMAP
}

float4 CalculateReflectionProbe(in EnvProbe probe, float3 P, float3 N, float3 R, float3 camera_position, float roughness)
{
    float4 ibl = float4(0.0, 0.0, 0.0, 0.0);

    const float lod = HYP_FMATH_SQR(roughness) * 7.0;

#ifndef ENV_PROBE_PARALLAX_CORRECTED
    // ENV_PROBE_PARALLAX_CORRECTED is not statically defined, we need to use flags on the EnvProbe struct
    // at render time to determine if the probe is parallax corrected.
    const bool is_parallax_corrected = bool(probe.flags & HYP_ENV_PROBE_PARALLAX_CORRECTED);

    if (is_parallax_corrected)
    {
        R = EnvProbeCoordParallaxCorrected(probe.world_position.xyz, probe.aabb_min.xyz, probe.aabb_max.xyz, P, R);
    }
#endif // ENV_PROBE_PARALLAX_CORRECTED

    ApplyReflectionProbe(probe.texture_index, probe.world_position.xyz, probe.aabb_min.xyz, probe.aabb_max.xyz, P, R, lod, ibl);

    return ibl;
}

#endif // HYP_DEFERRED_NO_ENV_PROBE

#ifndef HYP_DEFERRED_NO_RT_RADIANCE
#ifdef PATHTRACER
float4 CalculatePathTracing(float2 uv)
{
    return SAMPLE_TEXTURE_2D_LOD(sampler_linear, RTRadianceResultTexture, uv, 0.0);
}
#endif // PATHTRACER

#ifdef RT_REFLECTIONS
void CalculateRayTracingReflection(float2 uv, inout float4 reflections)
{
    float4 rt_radiance = SAMPLE_TEXTURE_2D_LOD(sampler_linear, RTRadianceResultTexture, uv, 0.0);

    reflections = reflections * (1.0 - rt_radiance.a) + float4(rt_radiance.rgb, 1.0) * rt_radiance.a;
}
#endif // RT_REFLECTIONS
#endif // HYP_DEFERRED_NO_RT_RADIANCE

void IntegrateReflections(inout float3 Fr, in float4 reflections)
{
    Fr = (Fr * (1.0 - reflections.a)) + (reflections.rgb);
}

#endif // DEFERRED_LIGHTING_HLSLI