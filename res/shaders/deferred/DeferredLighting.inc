#ifndef HYP_DEFERRED_LIGHTING
#define HYP_DEFERRED_LIGHTING

#include "../include/shared.inc"
#include "../include/brdf.inc"
#include "../include/Octahedron.inc"

#define HYP_HBIL_MULTIPLIER 1.0
#define ENV_GRID_MULTIPLIER 1.0

struct Refraction
{
    vec3 position;
    vec3 direction;
};

void RefractionSolidSphere(
    vec3 P, vec3 N, vec3 V, float eta_ir,
    out Refraction out_refraction)
{
    const float thickness = 0.8;

    const vec3 R = refract(-V, N, eta_ir);
    float NdotR = dot(N, R);
    float d = thickness * -NdotR;
    vec3 n1 = normalize(NdotR * R - N * 0.5);

    Refraction refraction;
    refraction.position = vec3(P + R * d);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}

#ifndef HYP_DEFERRED_NO_REFRACTION

// Compute attenuated light as it travels through a volume.
vec3 ApplyVolumeAttenuation(vec3 radiance, float transmission_distance, vec3 attenuation_color, float attenuation_distance)
{
    if (attenuation_distance == 0.0)
    {
        // Attenuation distance is +∞ (which we indicate by zero), i.e. the transmitted color is not attenuated at all.
        return radiance;
    }
    else
    {
        // Compute light attenuation using Beer's law.
        vec3 attenuation_coefficient = -log(attenuation_color) / attenuation_distance;
        vec3 transmittance = exp(-attenuation_coefficient * transmission_distance); // Beer's law
        return transmittance * radiance;
    }
}

vec3 CalculateRefraction(
    uvec2 image_dimensions,
    vec3 P, vec3 N, vec3 V, vec2 texcoord,
    vec3 F0, vec3 E,
    float transmission, float roughness,
    vec4 opaque_color, vec4 translucent_color,
    vec3 brdf)
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

#ifdef LANG_GLSL
    vec4 refraction_pos = camera.viewProjMat * vec4(refraction.position, 1.0);
#elif defined(LANG_HLSL)
    float4 refraction_pos = mul(camera.viewProjMat, vec4(refraction.position, 1.0));
#endif

    refraction_pos /= refraction_pos.w;

    vec2 refraction_texcoord = refraction_pos.xy * 0.5 + 0.5;

    const float lod = ApplyIORToRoughness(IOR, roughness) * log2(float(max_dimension));

    float absorption = 0.1; // TODO: material parameter
    vec3 T = min(vec3(1.0, 1.0, 1.0), exp(-absorption * refraction.direction));

    vec3 Ft = SAMPLE_TEXTURE_2D_LOD(sampler_linear, gbuffer_mip_chain, refraction_texcoord, lod).rgb;
    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E;
    Ft *= T;

    return Ft;
}

#endif

#ifndef HYP_DEFERRED_NO_ENV_PROBE

#include "../include/env_probe.inc"

vec3 SampleEnvProbe_SH(uint env_probe_index, vec3 N)
{
    SH9 sh9;

    for (int i = 0; i < 9; i++)
    {
        sh9.values[i] = env_probes[env_probe_index].sh[i].rgb;
    }

    return SphericalHarmonicsSample(sh9, N);
}

void ApplyReflectionProbe(uint probe_texture_index, vec3 probe_world_position, vec3 aabb_min, vec3 aabb_max, vec3 P, vec3 R, float lod, inout vec4 ibl)
{
    ibl = vec4(0.0, 0.0, 0.0, 0.0);

    probe_texture_index = min(probe_texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1);

    const vec3 extent = (aabb_max - aabb_min);
    const vec3 center = (aabb_max + aabb_min) * 0.5;
    const vec3 diff = P - center;

#ifdef ENV_PROBE_PARALLAX_CORRECTED
    R = EnvProbeCoordParallaxCorrected(probe_world_position, aabb_min, aabb_max, P, R);
#endif

#if ENV_PROBE_CUBEMAP
    ibl = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(sampler_linear, envProbesTexture, vec4(normalize(R), float(probe_texture_index)), lod);
#else
    ibl = SAMPLE_TEXTURE_2D_ARRAY_LOD(sampler_linear, envProbesTexture, vec3(EncodeOctahedralCoord(normalize(R)) * 0.5 + 0.5, float(probe_texture_index)), lod);
#endif
}

vec4 CalculateReflectionProbe(in EnvProbe probe, vec3 P, vec3 N, vec3 R, vec3 camera_position, float roughness)
{
    vec4 ibl = vec4(0.0, 0.0, 0.0, 0.0);

    const float lod = HYP_FMATH_SQR(roughness) * 12.0;

#ifndef ENV_PROBE_PARALLAX_CORRECTED
    // ENV_PROBE_PARALLAX_CORRECTED is not statically defined, we need to use flags on the EnvProbe struct
    // at render time to determine if the probe is parallax corrected.
    const bool is_parallax_corrected = bool(probe.flags & HYP_ENV_PROBE_PARALLAX_CORRECTED);

    if (is_parallax_corrected)
    {
        R = EnvProbeCoordParallaxCorrected(probe.world_position.xyz, probe.aabb_min.xyz, probe.aabb_max.xyz, P, R);
    }
#endif

    ApplyReflectionProbe(probe.texture_index, probe.world_position.xyz, probe.aabb_min.xyz, probe.aabb_max.xyz, P, R, lod, ibl);

    return ibl;
}

#endif

#ifndef HYP_DEFERRED_NO_RT_RADIANCE
#ifdef PATHTRACER
vec4 CalculatePathTracing(vec2 uv)
{
    return SAMPLE_TEXTURE_2D_LOD(sampler_linear, rt_radiance_final, uv, 0.0);
}
#endif

#ifdef RT_REFLECTIONS
void CalculateRayTracingReflection(vec2 uv, inout vec4 reflections)
{
    vec4 rt_radiance = SAMPLE_TEXTURE_2D_LOD(sampler_linear, rt_radiance_final, uv, 0.0);

    reflections = reflections * (1.0 - rt_radiance.a) + vec4(rt_radiance.rgb, 1.0) * rt_radiance.a;
}
#endif
#endif

void CalculateHBILIrradiance(in vec4 ssao_data, inout vec3 irradiance)
{
    irradiance += ssao_data.rgb * HYP_HBIL_MULTIPLIER;
}

void IntegrateReflections(inout vec3 Fr, in vec4 reflections)
{
    Fr = (Fr * (1.0 - reflections.a)) + (reflections.rgb);
}

#endif