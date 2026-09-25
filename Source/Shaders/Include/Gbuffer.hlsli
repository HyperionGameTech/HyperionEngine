#ifndef HYP_GBUFFER
#define HYP_GBUFFER

#include "Defines.hlsli"

#define HYP_GBUFFER_FLIP_Y 0

#include "Shared.hlsli"
#include "Packing.hlsli"

#define gbuffer_sampler sampler_linear
#define gbuffer_depth_sampler sampler_nearest

#ifndef sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#endif

#ifndef HYP_SAMPLER_LINEAR
#define HYP_SAMPLER_LINEAR sampler_linear
#endif

// Emissives
#define GBUFFER_EMISSIVE_SHIFT 8u
#define GBUFFER_EMISSIVE_MIN_LOG2 (-10.0)
#define GBUFFER_EMISSIVE_MAX_LOG2 (14.0)

struct GBufferMaterialParams
{
    float roughness;
    float metalness;
    uint mask;
};

void GBufferPackMaterialParams(GBufferMaterialParams params, out float roughnessAndMetalPacked, out uint mask)
{
    // max. 10 bits for roughness / metal packed - stored in normals target (r10g10b10a2)
    // params.roughness is alpha (perceptual squared); stored as perceptual so the 6 bits aren't all spent on rough values
    const float perceptualRoughness = sqrt(saturate(params.roughness));
    roughnessAndMetalPacked = float(HYP_QUANTIZE(perceptualRoughness, 6) | (HYP_QUANTIZE(params.metalness, 4) << 6)) / 1023.0;
    // mask: 4 bits
    mask = params.mask & 0xFu;
}

void GBufferUnpackMaterialParams(float roughnessAndMetalPacked, uint mask, out GBufferMaterialParams params)
{
    uint roughnessAndMetalU32 = uint(round(roughnessAndMetalPacked * 1023.0));

    const float perceptualRoughness = HYP_UNQUANTIZE(roughnessAndMetalU32 & 0x3Fu, 6);
    params.roughness = perceptualRoughness * perceptualRoughness;
    params.metalness = HYP_UNQUANTIZE((roughnessAndMetalU32 >> 6) & 0xFu, 4);
    
    params.mask = mask & 0xFu;
}

uint GBufferPackEmissive(float3 emissive)
{
    emissive = max(emissive, (float3)0.0);

    const float maxChannel = max(emissive.r, max(emissive.g, emissive.b));

    if (!(maxChannel >= exp2(GBUFFER_EMISSIVE_MIN_LOG2)))
    {
        return 0u;
    }

    const float logT = saturate((log2(maxChannel) - GBUFFER_EMISSIVE_MIN_LOG2) / (GBUFFER_EMISSIVE_MAX_LOG2 - GBUFFER_EMISSIVE_MIN_LOG2));
    const uint logBits = 1u + uint(round(logT * 254.0));

    const uint maxIndex = select((emissive.r >= emissive.g && emissive.r >= emissive.b), 0u, select(emissive.g >= emissive.b, 1u, 2u));
    
    const float2 others = maxIndex == 0u ? emissive.gb : (maxIndex == 1u ? emissive.rb : emissive.rg);
    const uint2 ratioBits = uint2(round(sqrt(saturate(others / maxChannel)) * 31.0));

    const uint packed = logBits | (maxIndex << 8u) | (ratioBits.x << 10u) | (ratioBits.y << 15u);

    return packed << GBUFFER_EMISSIVE_SHIFT;
}

float3 GBufferUnpackEmissive(uint materialBits)
{
    const uint packed = (materialBits >> GBUFFER_EMISSIVE_SHIFT) & 0xFFFFFu;
    const uint logBits = packed & 0xFFu;

    if (logBits == 0u)
    {
        return (float3)0.0;
    }

    const float maxChannel = exp2(lerp(GBUFFER_EMISSIVE_MIN_LOG2, GBUFFER_EMISSIVE_MAX_LOG2, float(logBits - 1u) / 254.0));

    const uint maxIndex = (packed >> 8u) & 0x3u;

    float2 others = float2(float((packed >> 10u) & 0x1Fu), float((packed >> 15u) & 0x1Fu)) / 31.0;
    others = others * others * maxChannel;

    return select(maxIndex == 0u, float3(maxChannel, others.x, others.y), select(maxIndex == 1u, float3(others.x, maxChannel, others.y), float3(others.x, others.y, maxChannel)));
}

vec4 GBufferPackNormal(vec3 normal)
{
    // use 2 bit w component for z value
    return vec4(0.0, EncodeNormal(normal).xyz);
}

vec3 GBufferUnpackNormal(vec4 packed)
{
    return DecodeNormal(packed.yzww);
}

#endif
