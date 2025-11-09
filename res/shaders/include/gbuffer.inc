#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#include "defines.inc"

#define HYP_GBUFFER_FLIP_Y 0

#include "shared.inc"
#include "packing.inc"

#define gbuffer_sampler sampler_linear
#define gbuffer_depth_sampler sampler_nearest

#ifndef sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#endif

#ifndef HYP_SAMPLER_LINEAR
#define HYP_SAMPLER_LINEAR sampler_linear
#endif

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
#define HYP_GBUFFER_TEXTURE(index) (gbuffer_textures[index])
#define gbuffer_albedo_texture HYP_GBUFFER_TEXTURE(0)
#define gbuffer_normals_texture HYP_GBUFFER_TEXTURE(1)
#define gbuffer_material_texture HYP_GBUFFER_TEXTURE(2)
#define gbuffer_velocity_texture HYP_GBUFFER_TEXTURE(3)
#define gbuffer_ws_normals_texture HYP_GBUFFER_TEXTURE(4)
#endif

struct GBufferMaterialParams
{
    float roughness;
    float metalness;
    float transmission;
    float ao;
    uint mask;
};

uvec2 GBufferPackMaterialParams(in GBufferMaterialParams params)
{
    uvec2 vec = uvec2(0);
    vec.x = HYP_QUANTIZE(params.roughness, 6)
        | (HYP_QUANTIZE(params.metalness, 6) << 6)
        | (HYP_QUANTIZE(params.transmission, 6) << 12)
        | (HYP_QUANTIZE(1.0 - params.ao, 4) << 18)
        | ((params.mask & 0x3FFu) << 22);

    return vec;
}

void GBufferUnpackMaterialParams(in uvec2 vec, out GBufferMaterialParams params)
{
    params.roughness = HYP_UNQUANTIZE(vec.x & 0x3Fu, 6);
    params.metalness = HYP_UNQUANTIZE((vec.x >> 6) & 0x3Fu, 6);
    params.transmission = HYP_UNQUANTIZE((vec.x >> 12) & 0x3Fu, 6);
    params.ao = 1.0 - HYP_UNQUANTIZE((vec.x >> 18) & 0xFu, 4);
    params.mask = (vec.x >> 22) & 0x3FFu;
}

vec2 GBufferPackNormal(in vec3 normal)
{
    return PackNormalVec2(normal);
}

vec3 GBufferUnpackNormal(in vec2 packed)
{
    return UnpackNormalVec2(packed);
}

vec4 GBufferPackNormalUV1(in vec3 normal, in vec2 uv1)
{
    vec2 packedNormal = PackNormalVec2(normal);
    return vec4(packedNormal, uv1);
}

void GBufferUnpackNormalUV1(in vec4 packed, out vec3 normal, out vec2 uv1)
{
    normal = UnpackNormalVec2(packed.xy);
    uv1 = packed.zw;
}

#endif
