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

    float4 aabb_max;
    float4 aabb_min;
    float4 world_position;

    uint texture_index; // point light shadow map probes - this is the index in the point shadow maps array
    uint flags;
    float camera_near;
    float camera_far;

    uint2 dimensions;
    uint2 visibility_bits;

    float4 sh[9];
};

struct EnvGrid
{
    float4 dummy;
};

struct SH9
{
    float3 values[9];
};

#define HYP_ENV_PROBE_PARALLAX_CORRECTED 0x1

float4 EnvProbeSample(
    sampler samp,
#if ENV_PROBE_CUBEMAP
    textureCubeArray tex,
#else
    texture2DArray tex,
#endif
    uint texture_index,
    float3 coord, float lod)
{
#if ENV_PROBE_CUBEMAP
    float4 color = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, float4(normalize(coord), float(texture_index)), lod);
#else
    float4 color = SAMPLE_TEXTURE_2D_ARRAY_LOD(samp, tex, float3(EncodeOctahedralCoord(normalize(coord)) * 0.5 + 0.5, float(texture_index)), lod);
#endif
    // color.rgb = pow(color.rgb, float3(2.2));
    return color;
}

float3 EnvProbeCoordParallaxCorrected(
    float3 probe_world_position,
    float3 aabb_min, float3 aabb_max,
    float3 P, float3 R)
{
    float3 rbmax = (aabb_max - P) / R;
    float3 rbmin = (aabb_min - P) / R;
    float3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    float3 box = P + R * correction;
    return box - probe_world_position;
}

float4 EnvProbeSampleParallaxCorrected(
    sampler samp,
#if ENV_PROBE_CUBEMAP
    textureCubeArray tex,
#else
    texture2DArray tex,
#endif
    in EnvProbe env_probe,
    float3 world, float3 R, float lod)
{
    uint probe_texture_index = env_probe.texture_index;

    float3 rbmax = (env_probe.aabb_max.xyz - world) / R;
    float3 rbmin = (env_probe.aabb_min.xyz - world) / R;
    float3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    float3 box = world + R * correction;
    float3 coord = box - env_probe.world_position.xyz;

#if ENV_PROBE_CUBEMAP
    return SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, float4(normalize(coord), float(probe_texture_index)), lod);
#else
    return SAMPLE_TEXTURE_2D_ARRAY_LOD(samp, tex, float3(EncodeOctahedralCoord(normalize(coord)) * 0.5 + 0.5, float(probe_texture_index)), lod);
#endif
}

void ProjectSHBands(float3 N, out float bands[9])
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

float3 SphericalHarmonicsSample(const in SH9 sh, float3 normal)
{
    float bands[9];
    ProjectSHBands(normal, bands);

    float3 result = sh.values[0] * bands[0]
        + sh.values[1] * bands[1]
        + sh.values[2] * bands[2]
        + sh.values[3] * bands[3]
        + sh.values[4] * bands[4]
        + sh.values[5] * bands[5]
        + sh.values[6] * bands[6]
        + sh.values[7] * bands[7]
        + sh.values[8] * bands[8];

    result = max(result, (float3)0.0);

    return result;
}

float3 EnvProbeSH(in EnvProbe envProbe, float3 N)
{
    SH9 sh;
    for (int i = 0; i < 9; i++)
    {
        sh.values[i] = envProbe.sh[i].xyz;
    }

    return SphericalHarmonicsSample(sh, N);
}

#endif