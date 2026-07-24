#ifndef ENV_PROBES_HLSLI
#define ENV_PROBES_HLSLI

#include "Defines.hlsli"

#define EPT_SKY 0
#define EPT_REFLECTION 1
#define EPT_AMBIENT 2
#define EPT_INVALID (~0u)

// ENV PROBES

struct EnvProbe
{
    float4 aabb_max;
    float4 aabb_min;
    float4 world_position;

    uint2 dimensions;
    uint textureIndices; // High 16 bits == vis tex index, Low 16 bits == Color tex index
    uint typeAndFlags;

    float4 sh[9];
};

struct ProbeVolume
{
    float4 dummy;
};

struct SH9
{
    float3 values[9];
};

#define EPF_NONE 0x0
#define EPF_PARALLAX_CORRECTED 0x1
#define EPF_BAKED 0x2
#define EPF_REALTIME 0x4
#define EPF_ORIGIN_FROM_CENTER 0x8
#define EPF_VISIBILITY 0x10
#define EPF_DIFFUSE 0x20

#define INVALID_ENV_PROBE_TEXTURE (0xFFFFu)

#define GET_ENV_PROBE_TYPE(envProbe) (envProbe.typeAndFlags & 0x7)
#define GET_ENV_PROBE_FLAGS(envProbe) ((envProbe.typeAndFlags >> 3))
#define GET_ENV_PROBE_COLOR_TEXTURE_INDEX(envProbe) (envProbe.textureIndices & 0xFFFFu)
#define GET_ENV_PROBE_VIS_TEXTURE_INDEX(envProbe) ((envProbe.textureIndices >> 16) & 0xFFFFu)

float4 EnvProbeSample(
    sampler samp,
    textureCubeArray tex,
    uint textureIndex,
    float3 coord, float lod)
{
    float4 color = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, float4(normalize(coord), float(textureIndex)), lod);
    // color.rgb = pow(color.rgb, float3(2.2));
    return color;
}

// @TODO Make configurable.
static const float s_envProbeBlendFactor = 0.1;

float CalculateEnvProbeWeight(float3 positionWS, float3 aabbMin, float3 aabbMax)
{
    const float3 aabbExtent = aabbMax - aabbMin;

    const float3 blend = aabbExtent * s_envProbeBlendFactor;
    const float3 distToMin = (positionWS.xyz - aabbMin) / blend;
    const float3 distToMax = (aabbMax - positionWS.xyz) / blend;
    const float minBlend = min(distToMin.x, min(distToMin.y, min(distToMin.z, min(distToMax.x, min(distToMax.y, distToMax.z)))));

    return smoothstep(0.0, 1.0, minBlend);
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
    textureCubeArray tex,
    in EnvProbe envProbe,
    float3 world, float3 R, float lod)
{
    uint colorTextureIndex = GET_ENV_PROBE_COLOR_TEXTURE_INDEX(envProbe);

    if (colorTextureIndex == INVALID_ENV_PROBE_TEXTURE)
    {
        return (float4)0.0;
    }

    float3 rbmax = (envProbe.aabb_max.xyz - world) / R;
    float3 rbmin = (envProbe.aabb_min.xyz - world) / R;
    float3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    float3 box = world + R * correction;
    float3 coord = box - envProbe.world_position.xyz;

    return SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, float4(normalize(coord), float(colorTextureIndex)), lod);
}

void ProjectSHBands(float3 N, out float bands[9])
{
    bands[0] = 0.282095;
    bands[1] = 0.488603 * N.y;
    bands[2] = 0.488603 * N.z;
    bands[3] = 0.488603 * N.x;
    bands[4] = 1.092548 * N.x * N.y;
    bands[5] = 1.092548 * N.y * N.z;
    bands[6] = 0.315392 * (3.0 * N.z * N.z - 1.0);
    bands[7] = 1.092548 * N.x * N.z;
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

// Sample spherical harmonics from an EnvProbe up to the given SH order
float3 EnvProbeSH(in EnvProbe envProbe, float3 N, int order = 2)
{
    float bands[9];
    ProjectSHBands(N, bands);

    const int numCoeffs = min((order + 1) * (order + 1), 9);

    float3 result = (float3)0.0;

    for (int i = 0; i < numCoeffs; i++)
    {
        result += envProbe.sh[i].rgb * bands[i];
    }

    return max(result, (float3)0.0);
}

#endif
