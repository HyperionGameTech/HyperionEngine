#ifndef HYP_ENV_PROBE_GLSL
#define HYP_ENV_PROBE_GLSL

#include "defines.inc"

// ENV PROBES

#define ENV_PROBE_TYPE_INVALID ~0u
#define ENV_PROBE_TYPE_SKY 0
#define ENV_PROBE_TYPE_REFLECTION 1
#define ENV_PROBE_TYPE_SHADOW 2
#define ENV_PROBE_TYPE_AMBIENT 3
#define ENV_PROBE_TYPE_MAX 4
#define ENV_PROBE_CUBEMAP 1

struct EnvProbe
{
    mat4 face_view_matrices[6];

    vec4 aabb_max;
    vec4 aabb_min;
    vec4 world_position;

    uint texture_index; // point light shadow map probes - this is the index in the point shadow maps array
    uint flags;
    float camera_near;
    float camera_far;

    uvec2 dimensions;
    uvec2 visibility_bits;
    ivec4 position_in_grid;

    vec4 sh[9];
};

struct EnvGrid
{
    vec4 dummy;
};

struct SH9
{
    vec3 values[9];
};

#define HYP_ENV_PROBE_PARALLAX_CORRECTED 0x1

vec4 EnvProbeSample(
    sampler samp,
#if ENV_PROBE_CUBEMAP
    textureCubeArray tex,
#else
    texture2DArray tex,
#endif
    uint texture_index,
    vec3 coord, float lod)
{
#if ENV_PROBE_CUBEMAP
    vec4 color = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, vec4(normalize(coord), float(texture_index)), lod);
#else
    vec4 color = SAMPLE_TEXTURE_2D_ARRAY_LOD(samp, tex, vec3(EncodeOctahedralCoord(normalize(coord)) * 0.5 + 0.5, float(texture_index)), lod);
#endif
    // color.rgb = pow(color.rgb, vec3(2.2));
    return color;
}

vec3 EnvProbeCoordParallaxCorrected(
    vec3 probe_world_position,
    vec3 aabb_min, vec3 aabb_max,
    vec3 P, vec3 R)
{
    vec3 rbmax = (aabb_max - P) / R;
    vec3 rbmin = (aabb_min - P) / R;
    vec3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = P + R * correction;
    return box - probe_world_position;
}

vec4 EnvProbeSampleParallaxCorrected(
    sampler samp,
#if ENV_PROBE_CUBEMAP
    textureCubeArray tex,
#else
    texture2DArray tex,
#endif
    in EnvProbe env_probe,
    vec3 world, vec3 R, float lod)
{
    uint probe_texture_index = env_probe.texture_index;

    vec3 rbmax = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction;
    vec3 coord = box - env_probe.world_position.xyz;

#if ENV_PROBE_CUBEMAP
    return SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, vec4(normalize(coord), float(probe_texture_index)), lod);
#else
    return SAMPLE_TEXTURE_2D_ARRAY_LOD(samp, tex, vec3(EncodeOctahedralCoord(normalize(coord)) * 0.5 + 0.5, float(probe_texture_index)), lod);
#endif
}

void ProjectSHBands(vec3 N, out float bands[9])
{
    bands[0] = 0.282095;
    bands[1] = -0.488603 * N.y;
    bands[2] = 0.488603 * N.z;
    bands[3] = -0.488603 * N.x;
    bands[4] = 1.092548 * N.x * N.y;
    bands[5] = -1.092548 * N.y * N.z;
    bands[6] = 0.315392 * (3.0 * N.z * N.z - 1.0);
    bands[7] = -1.092548 * N.x * N.z;
    bands[8] = 0.546274 * (N.x * N.x - N.y * N.y);
}

vec3 SphericalHarmonicsSample(const in SH9 sh, vec3 normal)
{
    // const float x = normal.x;
    // const float y = normal.y;
    // const float z = normal.z;

    // vec3 result = vec3(
    //     sh.values[0] +

    //     sh.values[1] * x + sh.values[2] * y + sh.values[3] * z +

    //     sh.values[4] * z * x + sh.values[5] * y * z + sh.values[6] * y * x + sh.values[7] * (3.0 * z * z - 1.0) + sh.values[8] * (x * x - y * y));

    float bands[9];
    ProjectSHBands(normal, bands);

    vec3 result = sh.values[0] * bands[0]
        + sh.values[1] * bands[1]
        + sh.values[2] * bands[2]
        + sh.values[3] * bands[3]
        + sh.values[4] * bands[4]
        + sh.values[5] * bands[5]
        + sh.values[6] * bands[6]
        + sh.values[7] * bands[7]
        + sh.values[8] * bands[8];

    result = max(result, vec3(0.0, 0.0, 0.0));

    return result;
}

#endif