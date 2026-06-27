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

struct GBufferMaterialParams
{
    float roughness;
    float metalness;
    uint mask;
};

void GBufferPackMaterialParams(GBufferMaterialParams params, out float roughnessAndMetalPacked, out uint mask)
{
    // max. 10 bits for roughness / metal packed - stored in normals target (r10g10b10a2)
    roughnessAndMetalPacked = float(HYP_QUANTIZE(params.roughness, 6) | (HYP_QUANTIZE(params.metalness, 4) << 6)) / 1023.0;
    // mask: 4 bits
    mask = params.mask & 0xFu;
}

void GBufferUnpackMaterialParams(float roughnessAndMetalPacked, uint mask, out GBufferMaterialParams params)
{
    uint roughnessAndMetalU32 = uint(round(roughnessAndMetalPacked * 1023.0));

    params.roughness = HYP_UNQUANTIZE(roughnessAndMetalU32 & 0x3Fu, 6);
    params.metalness = HYP_UNQUANTIZE((roughnessAndMetalU32 >> 6) & 0xFu, 4);
    
    params.mask = mask & 0xFu;
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
