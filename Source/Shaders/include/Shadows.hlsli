#ifndef HYP_SHADOWS
#define HYP_SHADOWS

#include "noise.inc"
#include "aabb.inc"

#define HYP_SHADOW_BIAS 0.005
//#define HYP_SHADOW_VARIABLE_BIAS 1
#define HYP_SHADOW_PENUMBRA_MIN 0.35
#define HYP_SHADOW_PENUMBRA_MAX 3.0

#define HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION 0.1

// #define HYP_SHADOW_SAMPLES_4
// #define HYP_SHADOW_SAMPLES_8
#define HYP_SHADOW_SAMPLES_16
// #define HYP_SHADOW_SAMPLES_36_TAP

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
    0.0, 0.5, 0.0, 0.5,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0);

float3 GetShadowCoord(in float4x4 shadowMatrix, float3 pos)
{
    float4x4 shadowMatrixBiased = mul(s_shadowBiasMatrix, shadowMatrix);
    
    float4 shadowPosition = mul(shadowMatrixBiased, float4(pos, 1.0));
    shadowPosition.xyz /= shadowPosition.w;

    return shadowPosition.xyz;
}

float GetShadowStandard(in ShadowMap shadowMap, float3 pos, float2 offset, float NdotL, uint cascadeIndex = 0)
{
    ShadowCascade cascade = shadowMap.cascades[cascadeIndex];

    const float2 offsetUV = float2(cascade.aabbMin.w, cascade.aabbMax.w);

    const float3 coord = GetShadowCoord(cascade.viewProjMat, pos);
    const float4 shadow_sample = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3((saturate(coord.xy + offset) * cascade.dimensionsScale.zw) + offsetUV, float(SHADOW_MAP_LAYER_INDEX(shadowMap, cascadeIndex))), 0);
    const float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.005);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowStandard(in ShadowMap shadowMap, float3 pos, uint cascadeIndex = 0)
{
    ShadowCascade cascade = shadowMap.cascades[cascadeIndex];

    const float3 coord = saturate(GetShadowCoord(cascade.viewProjMat, pos));
    const float4 shadow_sample = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3(coord.xy * cascade.dimensionsScale.zw, float(SHADOW_MAP_LAYER_INDEX(shadowMap, cascadeIndex))), 0);
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

float GetShadowPCF(in ShadowMap shadowMap, float3 pos, float2 texcoord, float2 screen_dimensions, float NdotL, uint cascadeIndex = 0)
{
#define HYP_1_OVER_16 0.0625

    ShadowCascade cascade = shadowMap.cascades[cascadeIndex];

    AABB aabb;
    aabb.min = cascade.aabbMin.xyz;
    aabb.max = cascade.aabbMax.xyz;

    const float4x4 shadowMatrix = cascade.viewProjMat;
    
    const float2 offsetUV = float2(cascade.aabbMin.w, cascade.aabbMax.w);

    const float2 dimensions = cascade.dimensionsScale.xy;
    const float2 uv_scale = cascade.dimensionsScale.zw;

    if (!AABBContainsPoint(aabb, pos))
    {
        return 1.0;
    }

    const float3 coord = GetShadowCoord(shadowMatrix, pos);

    const float2 texel_size = float2(1.0, 1.0) / dimensions;

    const float2 a = frac(dimensions * coord.xy);
    const float2 b = float2(1.0, 1.0) - a;

    const float shadow_map_depth = coord.z;
    const float shadow_filter_size = 0.0015;

    const float gradient_noise = InterleavedGradientNoise(texcoord * screen_dimensions - 0.5);

#if defined(HYP_SHADOW_SAMPLES_16)
#define HYP_SHADOW_SAMPLE_COUNT 16
#elif defined(HYP_SHADOW_SAMPLES_8)
#define HYP_SHADOW_SAMPLE_COUNT 8
#elif defined(HYP_SHADOW_SAMPLES_4)
#define HYP_SHADOW_SAMPLE_COUNT 4
#endif

#ifdef HYP_SHADOW_SAMPLE_COUNT
#define HYP_DEF_VOGEL_DISK(iter_index) \
    float2 vogel_##iter_index = VogelDisk(iter_index, HYP_SHADOW_SAMPLE_COUNT, gradient_noise)

    HYP_DEF_VOGEL_DISK(0);
    HYP_DEF_VOGEL_DISK(1);
    HYP_DEF_VOGEL_DISK(2);
    HYP_DEF_VOGEL_DISK(3);

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(4);
    HYP_DEF_VOGEL_DISK(5);
    HYP_DEF_VOGEL_DISK(6);
    HYP_DEF_VOGEL_DISK(7);
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    // TODO: Precalculate vogel disk for a a set number of random values.
    HYP_DEF_VOGEL_DISK(8);
    HYP_DEF_VOGEL_DISK(9);
    HYP_DEF_VOGEL_DISK(10);
    HYP_DEF_VOGEL_DISK(11);
    HYP_DEF_VOGEL_DISK(12);
    HYP_DEF_VOGEL_DISK(13);
    HYP_DEF_VOGEL_DISK(14);
    HYP_DEF_VOGEL_DISK(15);
#endif

#undef HYP_DEF_VOGEL_DISK

    float3 shadow_coords[HYP_SHADOW_SAMPLE_COUNT];
    float4 shadow_samples[HYP_SHADOW_SAMPLE_COUNT];

#define HYP_FETCH_SHADOW(iter_index)                                                                                                                                                        \
    {                                                                                                                                                                                       \
        shadow_coords[iter_index] = GetShadowCoord(shadowMatrix, pos);                                                                                                                      \
        shadow_samples[iter_index] = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3((vogel_##iter_index * shadow_filter_size * uv_scale) + offsetUV, float(layer_index)), 0); \
    }

#define HYP_DO_SHADOW(iter_index)                                                                      \
    {                                                                                                  \
        shadowness += GetShadowStandard(shadowMap, pos, (vogel_##iter_index * shadow_filter_size), NdotL); \
    }

#endif

    float shadowness = 0.0;

#ifdef HYP_SHADOW_SAMPLES_36_TAP
    const int num_samples = 16;
    const int sqrt_num_samples = 4;
    const int radius = sqrt_num_samples - 1;

    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            float2 weights = float2(1.0);

            bvec2 is_first = bvec2(1.0 - step(float2(-radius + 1), float2(x, y)));
            bvec2 is_last = bvec2(step(float2(radius), float2(x, y)));

            weights = mix(weights, b, is_first);
            weights = mix(weights, a, is_last);

            const float weight = weights.x * weights.y;

            shadowness += GetShadowStandard(shadowMap, pos, (float2(x, y) * texel_size), NdotL) * weight;
        }
    }

    shadowness /= 36.0;

#else

#if 0

    HYP_FETCH_SHADOW(0) HYP_FETCH_SHADOW(1)
    HYP_FETCH_SHADOW(2) HYP_FETCH_SHADOW(3)

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW(4) HYP_FETCH_SHADOW(5)
    HYP_FETCH_SHADOW(6) HYP_FETCH_SHADOW(7)
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW(8)  HYP_FETCH_SHADOW(9)
    HYP_FETCH_SHADOW(10) HYP_FETCH_SHADOW(11)
    HYP_FETCH_SHADOW(12) HYP_FETCH_SHADOW(13)
    HYP_FETCH_SHADOW(14) HYP_FETCH_SHADOW(15)
#endif

#endif

    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(1)
    HYP_DO_SHADOW(2)
    HYP_DO_SHADOW(3)

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(5)
    HYP_DO_SHADOW(6)
    HYP_DO_SHADOW(7)
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(9)
    HYP_DO_SHADOW(10)
    HYP_DO_SHADOW(11)
    HYP_DO_SHADOW(12)
    HYP_DO_SHADOW(13)
    HYP_DO_SHADOW(14)
    HYP_DO_SHADOW(15)

    shadowness /= 16.0;

#elif defined(HYP_SHADOW_SAMPLES_8)
    shadowness /= 8.0;
#elif defined(HYP_SHADOW_SAMPLES_4)
    shadowness /= 4.0;
#endif

#endif

#undef HYP_DO_SHADOW

    return shadowness;

#undef HYP_1_OVER_16
}

float GetShadowVariance(in ShadowMap shadowMap, float3 pos, float NdotL, uint cascadeIndex = 0)
{
    ShadowCascade cascade = shadowMap.cascades[cascadeIndex];

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    const float2 offsetUV = float2(cascade.aabbMin.w, cascade.aabbMax.w);

    const float3 coord = GetShadowCoord(cascade.viewProjMat, pos);
    float2 moments = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(coord.xy * cascade.dimensionsScale.zw + offsetUV, float(SHADOW_MAP_LAYER_INDEX(shadowMap, cascadeIndex))), 0).xy;

    float d = coord.z - moments.x;
    float p = step(coord.z, moments.x + bias);
    float variance = max(moments.y - moments.x * moments.x, 0.0001);

    float p_max = variance / (variance + d * d);

    AABB aabb;
    aabb.min = cascade.aabbMin.xyz;
    aabb.max = cascade.aabbMax.xyz;

    return AABBContainsPoint(aabb, pos) ? max(p, p_max) : 1.0;

#if 0
    const float3 coord = GetShadowCoord(cascade.viewProjMat, pos);
    const float4 shadow_sample = SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_LINEAR, shadow_maps, float3(coord.xy * cascade.dimensionsScale.zw + offsetUV, float(SHADOW_MAP_LAYER_INDEX(shadowMap, cascadeIndex))), 0);
    const float moment = shadow_sample.r;

    if (coord.z <= moment) {
        return 1.0;
    }

    const float moment2 = shadow_sample.g;

    const float variance = max(moment2 - HYP_FMATH_SQR(moment), HYP_FMATH_EPSILON);
    const float d = coord.z - moment;
    
    float percent_in_shadow = variance / (variance + HYP_FMATH_SQR(d));
    percent_in_shadow = smoothstep(HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION, 1.0, percent_in_shadow);

    percent_in_shadow *= 1.0 - float(NdotL > 1.5708);
    return float(acos(NdotL));

#endif
}

float AvgBlockerDepthToPenumbra(float light_size, float avg_blocker_depth, float shadow_map_coord_z)
{
    float penumbra = (shadow_map_coord_z - avg_blocker_depth) * light_size / avg_blocker_depth;
    penumbra += HYP_SHADOW_PENUMBRA_MIN;
    penumbra = min(HYP_SHADOW_PENUMBRA_MAX, penumbra);
    return penumbra;
}

float GetShadowContactHardened(in ShadowMap shadowMap, float3 pos, float2 texcoord, float2 screen_dimensions, float NdotL, uint cascadeIndex = 0)
{
    ShadowCascade cascade = shadowMap.cascades[cascadeIndex];

    AABB aabb;
    aabb.min = cascade.aabbMin.xyz;
    aabb.max = cascade.aabbMax.xyz;

    if (!AABBContainsPoint(aabb, pos))
    {
        return 1.0;
    }

    const float2 uv_scale = cascade.dimensionsScale.zw;

    const float3 coord = GetShadowCoord(cascade.viewProjMat, pos);

    const uint layer_index = SHADOW_MAP_LAYER_INDEX(shadowMap, cascadeIndex);

    const float2 offsetUV = float2(cascade.aabbMin.w, cascade.aabbMax.w);

    const float shadow_map_depth = coord.z;

    const float shadow_filter_size = 0.001;
    const float penumbra_filter_size = 0.0005;
    const float light_size = 3.0; // affects how quickly shadows become soft

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
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3(((coord.xy + (vogel_##iter_index0 * s_pcfKernel[iter_index0] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layer_index)), 0).r,   \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3(((coord.xy + (vogel_##iter_index1 * s_pcfKernel[iter_index1] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layer_index)), 0).r,   \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3(((coord.xy + (vogel_##iter_index2 * s_pcfKernel[iter_index2] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layer_index)), 0).r,   \
            SAMPLE_TEXTURE_2D_ARRAY_LOD(HYP_SAMPLER_NEAREST, shadow_maps, float3(((coord.xy + (vogel_##iter_index3 * s_pcfKernel[iter_index3] * penumbra_filter_size)) * uv_scale) + offsetUV, float(layer_index)), 0).r);  \
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

float GetPointShadowVariance(uint layerIndex, float3 world_to_light, float NdotL)
{
    const float2 moments = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(HYP_SAMPLER_LINEAR, point_shadow_maps, float4(world_to_light, float(layerIndex)), 0).rg;
    const float current_depth = length(world_to_light);

    float shadow_dist = current_depth - moments.x;
    float p = step(current_depth, moments.x);
    float variance = max(moments.y - moments.x * moments.x, 0.0001);
    float p_max = variance / (variance + shadow_dist * shadow_dist);

    return max(p, p_max);
}

float GetPointShadowStandard(uint layerIndex, float3 world_to_light, float NdotL)
{
    const float shadow_depth = SAMPLE_TEXTURE_CUBE_ARRAY_LOD(HYP_SAMPLER_NEAREST, point_shadow_maps, float4(world_to_light, float(layerIndex)), 0).r;
    const float current_depth = length(world_to_light);

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.005);
#endif

    return max(step(current_depth - bias, shadow_depth), 0.0);
}

float GetPointShadow(in ShadowMap shadowMap, float3 world_to_light, float NdotL)
{
    uint layerIndex = SHADOW_MAP_LAYER_INDEX(shadowMap, 0);
    uint flags = shadowMap.flags;

    switch (flags & LF_SHADOW_FILTER_MASK)
    {
    case LF_SHADOW_VSM:
        return GetPointShadowVariance(layerIndex, world_to_light, NdotL);
    default:
        return GetPointShadowStandard(layerIndex, world_to_light, NdotL);
    }
}

float GetShadow(in ShadowMap shadowMap, float3 position, float2 texcoord, float2 screen_dimensions, float NdotL, uint cascadeIndex = 0)
{
    switch (shadowMap.flags & LF_SHADOW_FILTER_MASK)
    {
    case LF_SHADOW_VSM:
        return GetShadowVariance(shadowMap, position, NdotL, cascadeIndex);
    case LF_SHADOW_CONTACT_HARDENING:
        return GetShadowContactHardened(shadowMap, position, texcoord, screen_dimensions, NdotL, cascadeIndex);
    case LF_SHADOW_PCF:
        return GetShadowPCF(shadowMap, position, texcoord, screen_dimensions, NdotL, cascadeIndex);
    default:
        return GetShadowStandard(shadowMap, position, float2(0.0, 0.0), NdotL, cascadeIndex);
    }
}

#endif