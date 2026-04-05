#ifndef ENV_GRID_RADIANCE_GLSL
#define ENV_GRID_RADIANCE_GLSL

#include "../include/BRDF.hlsli"
#include "../include/noise.inc"
#include "../include/BlueNoise.inc"
#include "../include/Octahedron.inc"

//#define HYP_VCT_USE_ROUGHNESS_SCATTERING

vec4 FetchVoxel(vec3 position, float lod)
{
    vec4 rgba = textureLod(sampler3D(voxel_image, sampler_linear), position, lod);
    // rgba.rgb = pow(rgba.rgb, vec3(2.2));
    return rgba;
}

vec3 VctWorldToLocal(vec3 world_position, in AABB voxel_grid_aabb)
{
    const vec3 voxel_grid_aabb_min = vec3(min(voxel_grid_aabb.min.x, min(voxel_grid_aabb.min.y, voxel_grid_aabb.min.z)));
    const vec3 voxel_grid_aabb_max = vec3(max(voxel_grid_aabb.max.x, max(voxel_grid_aabb.max.y, voxel_grid_aabb.max.z)));
    const vec3 voxel_grid_aabb_extent = voxel_grid_aabb_max - voxel_grid_aabb_min;
    const vec3 voxel_grid_aabb_center = voxel_grid_aabb_min + voxel_grid_aabb_extent * 0.5;

    return (world_position - voxel_grid_aabb_center) / voxel_grid_aabb_extent;
}

vec3 VctLocalToTexCoord(vec3 local_position)
{
    return clamp(local_position + 0.5, vec3(0.0), vec3(1.0));
}

vec3 VctWorldToTexCoord(vec3 world_position, in AABB voxel_grid_aabb)
{
    return VctLocalToTexCoord(VctWorldToLocal(world_position, voxel_grid_aabb));
}

vec4 ConeTrace(in AABB voxel_grid_aabb, float voxel_size, vec3 origin, vec3 dir, float ratio, float max_dist, float sampling_factor)
{
    const float min_diameter = voxel_size;
    const float min_diameter_inv = 1.0 / min_diameter;

    vec4 accum = vec4(0.0);
    float dist = voxel_size * 2.0;

    uint iter = 0;
    const uint max_iterations = 512;

    while (dist < max_dist && accum.a < 1.0 && iter < max_iterations) {
        float diameter = max(min_diameter, dist * ratio);
        float lod = log2(diameter * min_diameter_inv);

        vec3 sample_pos = origin + dir * dist;

        vec4 voxel_color = FetchVoxel(sample_pos, lod);
        // voxel_color.rgb *= mix(1.0, voxel_color.a, float(include_lighting));
        voxel_color.rgb *= 1.0 - clamp(dist / max_dist, 0.0, 1.0);

        float weight = (1.0 - accum.a);
        accum += voxel_color * weight;

        const float prev_dist = dist;
        dist += diameter * sampling_factor;

        iter++;
    }
    
    return accum;
}

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, vec3 V, float roughness, in AABB voxel_grid_aabb)
{
    const float greatest_extent = 256.0;
    const float voxel_size = 1.0 / greatest_extent;

    return ConeTrace(voxel_grid_aabb, voxel_size, VctWorldToTexCoord(P, voxel_grid_aabb) + (N * voxel_size), R, RoughnessToConeAngle(roughness), 0.55 /* max distance */, 0.5 /* sampling factor */);
}

vec4 ConeTraceDiffuse(vec3 P, vec3 N, in AABB voxel_grid_aabb)
{
    const float greatest_extent = 256.0;
    const float voxel_size = 1.0 / greatest_extent;

    return ConeTrace(voxel_grid_aabb, voxel_size, VctWorldToTexCoord(P, voxel_grid_aabb) + (N * voxel_size), N, /*ratio*/0.6, 0.55 /* max distance */, 0.5 /* sampling factor */);
}

vec4 ComputeVoxelRadiance(vec3 world_position, vec3 N, vec3 V, float roughness, uvec2 pixel_coord, uvec2 screen_resolution, uint frame_counter, ivec3 grid_size, in AABB voxel_grid_aabb)
{
    roughness = clamp(roughness, 0.01, 0.99);

#ifdef HYP_VCT_USE_ROUGHNESS_SCATTERING
    vec2 blue_noise_sample = vec2(
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 0),
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 1)
    );

    vec2 blue_noise_scaled = blue_noise_sample + float(frame_counter % 256) * 1.618;
    vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    vec3 H = ImportanceSampleGGX(rnd, N, roughness);
    H = tangent * H.x + bitangent * H.y + N * H.z;

    const vec3 R = normalize(reflect(-V, H));
#else
    const vec3 R = normalize(reflect(-V, N));
#endif
    
    return ConeTraceSpecular(world_position, N, R, V, roughness, voxel_grid_aabb);
}

vec4 ComputeVoxelIrradiance(vec3 world_position, vec3 N, uvec2 pixel_coord, uvec2 screen_resolution, uint frame_counter, ivec3 grid_size, in AABB voxel_grid_aabb)
{
    return ConeTraceDiffuse(world_position, N, voxel_grid_aabb);
}

#endif // ENV_GRID_RADIANCE_GLSL