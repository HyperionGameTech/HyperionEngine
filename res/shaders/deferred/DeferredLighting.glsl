#ifndef HYP_DEFERRED_LIGHTING_GLSL
#define HYP_DEFERRED_LIGHTING_GLSL

#include "../include/shared.inc"
#include "../include/brdf.inc"
#include "../include/Octahedron.glsl"

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

    vec4 refraction_pos = camera.projection * camera.view * vec4(refraction.position, 1.0);
    refraction_pos /= refraction_pos.w;

    vec2 refraction_texcoord = refraction_pos.xy * 0.5 + 0.5;

    const float lod = ApplyIORToRoughness(IOR, roughness) * log2(float(max_dimension));

    float absorption = 0.1; // TODO: material parameter
    vec3 T = min(vec3(1.0), exp(-absorption * refraction.direction));

    vec3 Ft = Texture2DLod(sampler_linear, gbuffer_mip_chain, refraction_texcoord, lod).rgb;
    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E;
    Ft *= T;

    return Ft;
}

#endif

#ifndef HYP_DEFERRED_NO_ENV_PROBE

#include "../include/env_probe.inc"

#ifndef HYP_DEFERRED_NO_ENV_GRID

ivec2 GetEnvProbeLightFieldOffset(ivec3 probe_grid_position)
{
    return ivec2(
        (env_grid.irradiance_octahedron_size.x + 2) * (float(probe_grid_position.x) * float(env_grid.density.y) + float(probe_grid_position.y)) + 1,
        (env_grid.irradiance_octahedron_size.y + 2) * float(probe_grid_position.z) + 1);
}

vec2 GetEnvProbeLightFieldUV(ivec3 probe_grid_position, vec3 N)
{
    // vec2 oct_coord = EncodeOctahedralCoord(normalize(N)) * 0.5 + 0.5;
    // oct_coord *= vec2(env_grid.irradiance_octahedron_size - 1) / vec2(env_grid.light_field_image_dimensions - ivec2(2));

    // ivec2 offset_coord = GetEnvProbeLightFieldOffset(probe_grid_position);
    // vec2 offset_uv = vec2(offset_coord) / vec2(env_grid.light_field_image_dimensions - ivec2(2));

    // return offset_uv + oct_coord;

    vec2 oct_coord = EncodeOctahedralCoord(normalize(N)) * 0.5 + 0.5;
    oct_coord *= vec2(env_grid.irradiance_octahedron_size) / vec2(env_grid.light_field_image_dimensions);

    ivec2 offset_coord = GetEnvProbeLightFieldOffset(probe_grid_position);
    vec2 offset_uv = vec2(offset_coord) / vec2(env_grid.light_field_image_dimensions);

    return offset_uv + oct_coord;
}

vec3 SampleEnvProbe_SH(uint env_probe_index, vec3 N)
{
    SH9 sh9;

    for (int i = 0; i < 9; i++)
    {
        sh9.values[i] = env_probes[env_probe_index].sh[i].rgb;
    }

    return SphericalHarmonicsSample(sh9, N);
}

vec3 SampleEnvProbe_LightField(uint env_probe_index, vec3 N)
{
    //vec2 uv = GetEnvProbeLightFieldUV(env_probes[env_probe_index].position_in_grid.xyz, N);
    //vec4 color = Texture2D(sampler_linear, light_field_color_texture, uv);
    vec3 color = vec3(0.0, 1.0, 0.0); // Removed; will come back to this

    return color.rgb;
}

vec3 SampleEnvProbe(uint env_probe_index, vec3 N)
{
#if defined(IRRADIANCE_MODE_SH)
    return SampleEnvProbe_SH(env_probe_index, N);
#elif defined(IRRADIANCE_MODE_LIGHT_FIELD)
    return SampleEnvProbe_LightField(env_probe_index, N);
#else
    return vec3(0.0);
#endif
}

/// \todo : Optimize me - entire EnvProbe entity being copied with each struct member being copied as a separate instruction.
vec3 CalculateEnvGridIrradiance(vec3 P, vec3 N, vec3 V)
{
    int base_probe_index_at_point = int(GetLocalEnvProbeIndex(env_grid, P));

    if (base_probe_index_at_point < 0 || base_probe_index_at_point >= HYP_MAX_BOUND_AMBIENT_PROBES)
    {
        return vec3(0.0);
    }

    const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_at_point);

    if (base_probe_index == ~0u)
    {
        return vec3(0.0);
    }

    const vec3 size_of_probe = env_grid.aabb_extent.xyz / vec3(env_grid.density.xyz);
    const vec3 base_probe_position = env_probes[base_probe_index].world_position.xyz;

    const vec3 alpha = clamp((P - base_probe_position) / size_of_probe, vec3(0.0), vec3(1.0));

    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;

    // iterate over probe cage
    for (int i = 0; i < 8; i++)
    {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

        // Add (size_of_probe * 0.5) to bias it to ensure the correct probe is selected
        vec3 neighbor_probe_position = base_probe_position + (size_of_probe * vec3(offset)) + (size_of_probe * 0.5);

        int neighbor_probe_index_at_point = int(GetLocalEnvProbeIndex(env_grid, neighbor_probe_position));

        if (neighbor_probe_index_at_point < 0 || neighbor_probe_index_at_point >= HYP_MAX_BOUND_AMBIENT_PROBES)
        {
            continue;
        }

        const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_at_point);

        if (neighbor_probe_index == ~0u)
        {
            continue;
        }

        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = 1.0;

        vec3 probe_to_point = P - env_probes[neighbor_probe_index].world_position.xyz;
        float distance_to_probe = length(probe_to_point);
        vec3 dir = normalize(-probe_to_point);

        vec3 color = vec3(0.0);

#ifdef IRRADIANCE_MODE_LIGHT_FIELD
        // backface test
        weight *= max(0.05, dot(N, dir));

        vec2 uv = GetEnvProbeLightFieldUV(env_probes[neighbor_probe_index].position_in_grid.xyz, -dir);
        vec2 depths = texture(sampler2D(light_field_depth_texture, sampler_linear), uv).rg;

        float mean = depths.x;
        float variance = abs(depths.y - HYP_FMATH_SQR(mean));

        float chebyshev = variance / (variance + HYP_FMATH_SQR(max(distance_to_probe - mean, 0.0)));
        chebyshev = max(chebyshev * chebyshev * chebyshev, 0.0);
        weight *= (distance_to_probe <= mean) ? 1.0 : max(chebyshev, 0.0); // mix(chebyshev, 1.0, step(distance_to_probe, mean));
        weight = max(0.0001, weight);
#endif

        weight *= trilinear.x * trilinear.y * trilinear.z;

        color = SampleEnvProbe(neighbor_probe_index, N);

        total_irradiance += color * weight;
        total_weight += weight;
    }

    return total_irradiance / max(total_weight, 0.0001);
}

#endif

void ApplyReflectionProbe(uint probe_texture_index, vec3 probe_world_position, vec3 aabb_min, vec3 aabb_max, vec3 P, vec3 R, float lod, inout vec4 ibl)
{
    ibl = vec4(0.0);

    probe_texture_index = min(probe_texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1);

    const vec3 extent = (aabb_max - aabb_min);
    const vec3 center = (aabb_max + aabb_min) * 0.5;
    const vec3 diff = P - center;

#ifdef ENV_PROBE_PARALLAX_CORRECTED
    R = EnvProbeCoordParallaxCorrected(probe_world_position, aabb_min, aabb_max, P, R);
#endif

#if ENV_PROBE_CUBEMAP
    ibl = textureLod(samplerCubeArray(envProbesTexture, sampler_linear), vec4(normalize(R), float(probe_texture_index)), lod);
#else
    ibl = textureLod(sampler2DArray(envProbesTexture, sampler_linear), vec3(EncodeOctahedralCoord(normalize(R)) * 0.5 + 0.5, float(probe_texture_index)), lod);
#endif
}

vec4 CalculateReflectionProbe(in EnvProbe probe, vec3 P, vec3 N, vec3 R, vec3 camera_position, float roughness)
{
    vec4 ibl = vec4(0.0);

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
    return Texture2DLod(sampler_linear, rt_radiance_final, uv, 0.0);
}
#endif

#ifdef RT_REFLECTIONS
void CalculateRaytracingReflection(vec2 uv, inout vec4 reflections)
{
    vec4 rt_radiance = Texture2DLod(sampler_linear, rt_radiance_final, uv, 0.0);

    reflections = reflections * (1.0 - rt_radiance.a) + vec4(rt_radiance.rgb, 1.0) * rt_radiance.a;
}
#endif
#endif

void CalculateHBILIrradiance(in vec4 ssao_data, inout vec3 irradiance)
{
    irradiance += pow(ssao_data.rgb, vec3(2.2)) * HYP_HBIL_MULTIPLIER;
}

void IntegrateReflections(inout vec3 Fr, in vec4 reflections)
{
    Fr = (Fr * (1.0 - reflections.a)) + (reflections.rgb);
}

#endif