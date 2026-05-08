#ifndef HYP_SHADOWS
#define HYP_SHADOWS

#include "Noise.hlsli"
#include "Aabb.hlsli"

#define HYP_SHADOW_BIAS 0.001
// #define HYP_SHADOW_VARIABLE_BIAS 1
#define HYP_SHADOW_PENUMBRA_MIN 0.05
#define HYP_SHADOW_PENUMBRA_MAX 6.0

#define HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION 0.1

// #define HYP_SHADOW_SAMPLES_4
//#define HYP_SHADOW_SAMPLES_8
#define HYP_SHADOW_SAMPLES_16
//#define HYP_SHADOW_SAMPLES_36_TAP

// #define HYP_PENUMBRA_SAMPLES_4
#define HYP_PENUMBRA_SAMPLES_8

#define HYP_SHADOW_PENUMBRA 1
#define HYP_SHADOW_VARIANCE 1

static const float2 s_pcfKernel[16] = {
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

static const float4x4 s_shadowBiasMatrix = float4x4(
    0.5, 0.0, 0.0, 0.5,
    0.0, -0.5, 0.0, 0.5,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0);

float3 GetShadowCoord(in float4x4 shadowMatrix, float3 pos)
{
    float4x4 shadowMatrixBiased = mul(s_shadowBiasMatrix, shadowMatrix);

    float4 shadowPosition = mul(shadowMatrixBiased, float4(pos, 1.0));
    shadowPosition.xyz /= shadowPosition.w;

    return shadowPosition.xyz;
}

float GetShadowStandard(in ShadowMap shadowMap, float3 pos, float2 offset, float NdotL)
{
    const float2 offsetUV = float2(shadowMap.aabbMin.w, shadowMap.aabbMax.w);

    const float3 coord = GetShadowCoord(shadowMap.viewProjMat, pos);
    const float4 shadow_sample = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3((saturate(coord.xy + offset) * shadowMap.dimensionsScale.zw) + offsetUV, float(shadowMap.layerIndex)), 0);
    const float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.005);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowStandard(in ShadowMap shadowMap, float3 pos)
{
    const float3 coord = saturate(GetShadowCoord(shadowMap.viewProjMat, pos));
    const float4 shadow_sample = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(coord.xy * shadowMap.dimensionsScale.zw, float(shadowMap.layerIndex)), 0);
    const float shadow_depth = shadow_sample.r;

    const float bias = HYP_SHADOW_BIAS;

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowStandard(float4 shadow_sample, float3 coord, float NdotL)
{
    const float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowPCF(in ShadowMap shadowMap, float3 pos, float2 texcoord, float2 screen_dimensions, float NdotL)
{
    AABB aabb;
    aabb.min = shadowMap.aabbMin.xyz;
    aabb.max = shadowMap.aabbMax.xyz;

    const float4x4 shadowMatrix = shadowMap.viewProjMat;

    const float2 offsetUV = float2(shadowMap.aabbMin.w, shadowMap.aabbMax.w);

    const float2 dimensions = shadowMap.dimensionsScale.xy;
    const float2 uv_scale = shadowMap.dimensionsScale.zw;

    if (!AABBContainsPoint(aabb, pos))
    {
        return 1.0;
    }

    const float3 coord = GetShadowCoord(shadowMatrix, pos);

    const float shadow_filter_size = 0.001;

    float noise = InterleavedGradientNoise(texcoord * screen_dimensions - 0.5) * HYP_FMATH_TWO_PI;

    float s, c;
    sincos(noise, s, c);
    float2x2 rotationMatrix = float2x2(c, -s, s, c);

#if defined(HYP_SHADOW_SAMPLES_16)
#define HYP_SHADOW_SAMPLE_COUNT 16
#elif defined(HYP_SHADOW_SAMPLES_8)
#define HYP_SHADOW_SAMPLE_COUNT 8
#elif defined(HYP_SHADOW_SAMPLES_4)
#define HYP_SHADOW_SAMPLE_COUNT 4
#endif

#define HYP_DEF_VOGEL_DISK(iter_index) \
    float2 vogel_##iter_index = VogelDisk(iter_index, HYP_SHADOW_SAMPLE_COUNT / 4, noise)

    HYP_DEF_VOGEL_DISK(0);

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(1);
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(2);
    HYP_DEF_VOGEL_DISK(3);
#endif
#undef HYP_DEF_VOGEL_DISK

    float shadowness = 0.0;

#ifndef HYP_SAMPLER_SHADOW
#define HYP_USE_GATHER
#endif // HYP_SAMPLER_SHADOW

#ifdef HYP_USE_GATHER
    float4 shadow_samples[HYP_SHADOW_SAMPLE_COUNT / 4];
    int2 offsets[4] = {
        int2(0, 0),
        int2(2, 0),
        int2(0, 2),
        int2(2, 2)
    };

#define HYP_FETCH_SHADOW_SAMPLES(iter_index0)                                                                                         \
    {                                                                                                                               \
        float4 samples = shadow_maps.GatherRed(HYP_SAMPLER_LINEAR,                                                                  \
            float3(((coord.xy + (vogel_##iter_index0 * shadow_filter_size)) * uv_scale) + offsetUV, float(shadowMap.layerIndex)),   \
            offsets[iter_index0]);                                                                                                  \
        float4 deltas = max(step(coord.zzzz - (float4)HYP_SHADOW_BIAS, samples), (float4)0.0);                                      \
        shadow_samples[iter_index0] = deltas;                                                                                         \
    }

    HYP_FETCH_SHADOW_SAMPLES(0);

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW_SAMPLES(1);
#endif // HYP_SHADOW_SAMPLES_8 || HYP_SHADOW_SAMPLES_16

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW_SAMPLES(2);
    HYP_FETCH_SHADOW_SAMPLES(3);
#endif // HYP_USE_GATHER

    for (uint i = 0; i < HYP_SHADOW_SAMPLE_COUNT / 4; i++)
    {
        shadowness += shadow_samples[i].x + shadow_samples[i].y + shadow_samples[i].z + shadow_samples[i].w;
    }
#else // !HYP_USE_GATHER

    // Use SampleCmpLevelZero for hardware PCF

#define HYP_FETCH_SHADOW_SAMPLE(iter_index) \
    { \
        float2 rotatedOffset = mul(s_pcfKernel[iter_index], rotationMatrix); \
        float2 sampleUV = ((coord.xy + (rotatedOffset * shadow_filter_size)) * uv_scale) + offsetUV; \
        shadowness += shadow_maps.SampleCmpLevelZero(HYP_SAMPLER_SHADOW, float3(sampleUV, float(shadowMap.layerIndex)), coord.z - HYP_SHADOW_BIAS); \
    }

    HYP_FETCH_SHADOW_SAMPLE(0); HYP_FETCH_SHADOW_SAMPLE(1); HYP_FETCH_SHADOW_SAMPLE(2); HYP_FETCH_SHADOW_SAMPLE(3);

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW_SAMPLE(4); HYP_FETCH_SHADOW_SAMPLE(5); HYP_FETCH_SHADOW_SAMPLE(6); HYP_FETCH_SHADOW_SAMPLE(7);
#endif // HYP_SHADOW_SAMPLES_8 || HYP_SHADOW_SAMPLES_16

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW_SAMPLE(8); HYP_FETCH_SHADOW_SAMPLE(9); HYP_FETCH_SHADOW_SAMPLE(10); HYP_FETCH_SHADOW_SAMPLE(11);
    HYP_FETCH_SHADOW_SAMPLE(12); HYP_FETCH_SHADOW_SAMPLE(13); HYP_FETCH_SHADOW_SAMPLE(14); HYP_FETCH_SHADOW_SAMPLE(15);
#endif // HYP_SHADOW_SAMPLES_16

#endif // HYP_USE_GATHER

#undef HYP_DO_SHADOW

    shadowness *= (1.0 / float(HYP_SHADOW_SAMPLE_COUNT));

    return shadowness;
}

float AvgBlockerDepthToPenumbra(float light_size, float avg_blocker_depth, float shadow_map_coord_z)
{
    float penumbra = (shadow_map_coord_z - avg_blocker_depth) * light_size / avg_blocker_depth;
    penumbra += HYP_SHADOW_PENUMBRA_MIN;
    penumbra = min(HYP_SHADOW_PENUMBRA_MAX, penumbra);
    return penumbra;
}

float GetShadowContactHardened(in ShadowMap shadowMap, float3 pos, float2 texcoord, float2 screen_dimensions, float NdotL)
{
    AABB aabb;
    aabb.min = shadowMap.aabbMin.xyz;
    aabb.max = shadowMap.aabbMax.xyz;

    if (!AABBContainsPoint(aabb, pos))
    {
        return 1.0;
    }

    const float2 uv_scale = shadowMap.dimensionsScale.zw;

    const float3 coord = GetShadowCoord(shadowMap.viewProjMat, pos);

    const uint layerIndex = shadowMap.layerIndex;

    const float2 offsetUV = float2(shadowMap.aabbMin.w, shadowMap.aabbMax.w);

    const float shadow_map_depth = coord.z;

    const float shadow_filter_size = 0.002;
    const float penumbra_filter_size = 0.002;
    const float light_size = 18.0; // affects how quickly shadows become soft

    const float gradient_noise = InterleavedGradientNoise(texcoord * screen_dimensions - 0.5);

    float total_blocker_depth = 0.0;
    float num_blockers = 0.0;

#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_PENUMBRA_SAMPLES_16)
#define HYP_DEF_VOGEL_DISK(iter_index) \
    float2 vogel_##iter_index = VogelDisk(iter_index, 16, gradient_noise)
#else
#define HYP_DEF_VOGEL_DISK(iter_index) \
    float2 vogel_##iter_index = VogelDisk(iter_index, 8, gradient_noise)
#endif

    HYP_DEF_VOGEL_DISK(0);
    HYP_DEF_VOGEL_DISK(2);
    HYP_DEF_VOGEL_DISK(4);
    HYP_DEF_VOGEL_DISK(6);
    HYP_DEF_VOGEL_DISK(8);
    HYP_DEF_VOGEL_DISK(10);
    HYP_DEF_VOGEL_DISK(12);
    HYP_DEF_VOGEL_DISK(14);

#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_PENUMBRA_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(1);
    HYP_DEF_VOGEL_DISK(3);
    HYP_DEF_VOGEL_DISK(5);
    HYP_DEF_VOGEL_DISK(7);
    HYP_DEF_VOGEL_DISK(9);
    HYP_DEF_VOGEL_DISK(11);
    HYP_DEF_VOGEL_DISK(13);
    HYP_DEF_VOGEL_DISK(15);
#endif

#undef HYP_DEF_VOGEL_DISK

#if defined(HYP_SHADOW_PENUMBRA) && HYP_SHADOW_PENUMBRA

// vectorization
#define HYP_DO_SHADOW_PENUMBRA(iter_index0, iter_index1, iter_index2, iter_index3)                                                                                                                                          \
    {                                                                                                                                                                                                                       \
        float4 blocker_samples = float4(                                                                                                                                                                                    \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(((coord.xy + (vogel_##iter_index0 * s_pcfKernel[iter_index0] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layerIndex)), 0).r,    \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(((coord.xy + (vogel_##iter_index1 * s_pcfKernel[iter_index1] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layerIndex)), 0).r,    \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(((coord.xy + (vogel_##iter_index2 * s_pcfKernel[iter_index2] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layerIndex)), 0).r,    \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(((coord.xy + (vogel_##iter_index3 * s_pcfKernel[iter_index3] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layerIndex)), 0).r);   \
        float4 are_samples_blocking = float4(lessThan(blocker_samples, shadow_map_depth.xxxx));                                                                                                                             \
        total_blocker_depth += dot(blocker_samples * are_samples_blocking, float4(1.0, 1.0, 1.0, 1.0));                                                                                                                     \
        num_blockers += dot(are_samples_blocking, float4(1.0, 1.0, 1.0, 1.0));                                                                                                                                              \
    }

#ifdef HYP_PENUMBRA_SAMPLES_8
    HYP_DO_SHADOW_PENUMBRA(0, 2, 4, 6)
    HYP_DO_SHADOW_PENUMBRA(8, 10, 12, 14)
#elif defined(HYP_PENUMBRA_SAMPLES_4)
    HYP_DO_SHADOW_PENUMBRA(0, 4, 8, 12)
#endif

#undef HYP_DO_SHADOW_PENUMBRA

    float penumbra_mask = num_blockers > 0.0 ? AvgBlockerDepthToPenumbra(light_size, total_blocker_depth / max(num_blockers, 0.0001), shadow_map_depth) : 0.0;
#else
    float penumbra_mask = 1.0;
#endif

    float shadowness = 0.0;

#define HYP_DO_SHADOW(iter_index)                                                                                                 \
    {                                                                                                                             \
        shadowness += GetShadowStandard(shadowMap, pos, (vogel_##iter_index * penumbra_mask * shadow_filter_size * uv_scale), NdotL); \
    }

#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_SHADOW_SAMPLES_8)
    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(2)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(6)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(10)
    HYP_DO_SHADOW(12)
    HYP_DO_SHADOW(14)
#elif defined(HYP_SHADOW_SAMPLES_4)
    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(12)
#endif

#ifdef HYP_SHADOW_SAMPLES_16
    HYP_DO_SHADOW(1)
    HYP_DO_SHADOW(3)
    HYP_DO_SHADOW(5)
    HYP_DO_SHADOW(7)
    HYP_DO_SHADOW(9)
    HYP_DO_SHADOW(11)
    HYP_DO_SHADOW(13)
    HYP_DO_SHADOW(15)

    shadowness /= 16.0;
#elif defined(HYP_SHADOW_SAMPLES_8)
    shadowness /= 8.0;
#elif defined(HYP_SHADOW_SAMPLES_4)
    shadowness /= 4.0;
#endif

#undef HYP_DO_SHADOW

    return shadowness;
}

float GetPointShadowVariance(in ShadowMap shadowMap, float3 worldToLight, float NdotL)
{
    const uint layerIndex = shadowMap.layerIndex;

    const float2 moments = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(HYP_SAMPLER_LINEAR, point_shadow_maps, float4(worldToLight, float(layerIndex)), 0).rg;
    const float current_depth = length(worldToLight);

    float shadow_dist = current_depth - moments.x;
    float p = step(current_depth, moments.x);
    float variance = max(moments.y - moments.x * moments.x, 0.0001);
    float p_max = variance / (variance + shadow_dist * shadow_dist);

    return max(p, p_max);
}

float GetPointShadowStandard(in ShadowMap shadowMap, float3 worldToLight, float NdotL)
{
    const uint layerIndex = shadowMap.layerIndex;

    const float shadowDepth = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(HYP_SAMPLER_LINEAR, point_shadow_maps, float4(normalize(worldToLight), float(layerIndex)), 0).r;
    const float4 shadowPosVS = ReconstructViewSpacePositionFromDepth(shadowMap.invProjMat, float2(0.5, 0.5), shadowDepth);

    const float dist = max(max(abs(worldToLight.x), abs(worldToLight.y)), abs(worldToLight.z));

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.005);
#endif

    return max(step(dist - bias, shadowPosVS.z), 0.0);
}

float GetPointShadowPCF(in ShadowMap shadowMap, float3 worldToLight, float NdotL)
{
    const uint layerIndex = shadowMap.layerIndex;

    const float dist = max(max(abs(worldToLight.x), abs(worldToLight.y)), abs(worldToLight.z));

    const float shadow_filter_size = 0.05;

    float3 dir = normalize(worldToLight);
    float3 up = abs(dir.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, dir));
    float3 bitangent = cross(dir, tangent);

    float noise = InterleavedGradientNoise(dir.xy * 1000.0 - 0.5) * HYP_FMATH_TWO_PI;

#if defined(HYP_SHADOW_SAMPLES_16)
#define HYP_POINT_SHADOW_SAMPLE_COUNT 16
#elif defined(HYP_SHADOW_SAMPLES_8)
#define HYP_POINT_SHADOW_SAMPLE_COUNT 8
#else
#define HYP_POINT_SHADOW_SAMPLE_COUNT 4
#endif

    float shadowness = 0.0;

    [unroll]
    for (int i = 0; i < HYP_POINT_SHADOW_SAMPLE_COUNT; i++)
    {
        float2 vogel = VogelDisk(i, HYP_POINT_SHADOW_SAMPLE_COUNT, noise);
        float3 sampleDir = normalize(worldToLight + (tangent * vogel.x + bitangent * vogel.y) * shadow_filter_size);

        const float shadowDepth = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(HYP_SAMPLER_LINEAR, point_shadow_maps, float4(sampleDir, float(layerIndex)), 0).r;
        const float4 shadowPosVS = ReconstructViewSpacePositionFromDepth(shadowMap.invProjMat, float2(0.5, 0.5), shadowDepth);

        float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
        bias *= tan(acos(NdotL));
        bias = clamp(bias, 0.0, 0.005);
#endif

        shadowness += max(step(dist - bias, shadowPosVS.z), 0.0);
    }

    shadowness /= float(HYP_POINT_SHADOW_SAMPLE_COUNT);

#undef HYP_POINT_SHADOW_SAMPLE_COUNT

    return shadowness;
}

float GetPointShadow(in ShadowMap shadowMap, uint lightFlags, float3 worldToLight, float NdotL)
{
    switch (lightFlags & LF_SHADOW_FILTER_MASK)
    {
    case LF_SHADOW_VSM:
        return GetPointShadowVariance(shadowMap, worldToLight, NdotL);
    case LF_SHADOW_PCF:
        return GetPointShadowPCF(shadowMap, worldToLight, NdotL);
    default:
        return GetPointShadowStandard(shadowMap, worldToLight, NdotL);
    }
}

float GetShadow(in ShadowMap shadowMap, uint lightFlags, float3 position, float2 texcoord, float2 screen_dimensions, float NdotL)
{
    switch (lightFlags & LF_SHADOW_FILTER_MASK)
    {
    case LF_SHADOW_CONTACT_HARDENING:
        return GetShadowContactHardened(shadowMap, position, texcoord, screen_dimensions, NdotL);
    case LF_SHADOW_PCF:
        return GetShadowPCF(shadowMap, position, texcoord, screen_dimensions, NdotL);
    default:
        return GetShadowStandard(shadowMap, position, float2(0.0, 0.0), NdotL);
    }
}

#endif
