#ifndef HYP_MATERIAL
#define HYP_MATERIAL

#include "Defines.hlsli"
#include "Shared.hlsli"

// define textures

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#else // !HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Material, DiffuseMap) Texture2D DiffuseMap;
DECLARE_SRV(Material, NormalMap) Texture2D NormalMap;
DECLARE_SRV(Material, ParallaxMap) Texture2D ParallaxMap;
DECLARE_SRV(Material, MetalnessMap) Texture2D MetalnessMap;
DECLARE_SRV(Material, RoughnessMap) Texture2D RoughnessMap;
#endif // HYP_FEATURES_BINDLESS_TEXTURES

#endif // !HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

struct Material
{
    float4 albedo;

    uint4 packed_params;

    uint4 texture_indices[4];
    uint textureUsage;
    float parallax_height;
    float2 uv_scale;

    float4 _pad0;
};

// enum for packed params
#define MATERIAL_PARAM_ROUGHNESS 0
#define MATERIAL_PARAM_METALNESS 1
#define MATERIAL_PARAM_TRANSMISSION 2
#define MATERIAL_PARAM_ALPHA_THRESHOLD 3
#define MATERIAL_PARAM_EMISSIVE_COLOR 4 // r,g,b
#define MATERIAL_PARAM_EMISSIVE_INTENSITY 7
#define MATERIAL_PARAM_UI_BACKGROUND_COLOR 15

float UnpackMaterialParamFloat(uint4 uValue, uint index)
{
    return UINT_TO_VEC4(uValue[index >> 2])[index & 3];
}

float2 UnpackMaterialParamFloat2(uint4 uValue, uint index)
{
    float4 v = UINT_TO_VEC4(uValue[index >> 2]);
    return float2(v[index & 3], v[(index + 1) & 3]);
}

float3 UnpackMaterialParamFloat3(uint4 uValue, uint index)
{
    float4 v = UINT_TO_VEC4(uValue[index >> 2]);
    return float3(v[index & 3], v[(index + 1) & 3], v[(index + 2) & 3]);
}

float4 UnpackMaterialParamFloat4(uint4 uValue, uint index)
{
    float4 v = UINT_TO_VEC4(uValue[index >> 2]);
    // since we never have multiple params cross the boundary of the uint4, we can just return the whole thing rather than swizzling it.
    return v;
}

// Get float param
#define GET_MATERIAL_PARAM(mat, index) UnpackMaterialParamFloat((mat).packed_params, index)
#define GET_MATERIAL_PARAM_FLOAT2(mat, index) UnpackMaterialParamFloat2((mat).packed_params, index)
#define GET_MATERIAL_PARAM_FLOAT3(mat, index) UnpackMaterialParamFloat3((mat).packed_params, index)
#define GET_MATERIAL_PARAM_FLOAT4(mat, index) UnpackMaterialParamFloat4((mat).packed_params, index)

// Individual bits are stored in the last vector.
#define GET_MATERIAL_PARAM_BIT(mat, bitIndex) ((((mat).packed_params)[3]) & (1u << (bitIndex & 31)))

#define MATERIAL_FLAG_UNLIT 0
#define MATERIAL_FLAG_NORMAL_MAP_FLIP_Y 1
#define MATERIAL_CHANNEL_BIT_ROUGHNESS 2
#define MATERIAL_CHANNEL_BIT_METALNESS 4
#define MATERIAL_CHANNEL_BIT_AO 6
#define MATERIAL_FLAG_PARALLAX_INVERSE_HEIGHT 8

#define GET_MATERIAL_CHANNEL(mat, bitOffset) ((((mat).packed_params[3]) >> (bitOffset)) & 0x3u)

#define GET_MATERIAL_ROUGHNESS_CHANNEL(mat) GET_MATERIAL_CHANNEL(mat, MATERIAL_CHANNEL_BIT_ROUGHNESS)
#define GET_MATERIAL_METALNESS_CHANNEL(mat) GET_MATERIAL_CHANNEL(mat, MATERIAL_CHANNEL_BIT_METALNESS)
#define GET_MATERIAL_AO_CHANNEL(mat) GET_MATERIAL_CHANNEL(mat, MATERIAL_CHANNEL_BIT_AO)

float SelectMaterialChannel(float4 v, uint channel)
{
    return v[channel];
}

#define MATERIAL_TEXTURE_DiffuseMap 0
#define MATERIAL_TEXTURE_NormalMap 1
#define MATERIAL_TEXTURE_ParallaxMap 2
#define MATERIAL_TEXTURE_MetalnessMap 3
#define MATERIAL_TEXTURE_RoughnessMap 4
#define MATERIAL_TEXTURE_AoMap 5

#define MATERIAL_ALPHA_DISCARD 0.25

#define HAS_TEXTURE(mat, name) bool(((mat).textureUsage & (1u << MATERIAL_TEXTURE_##name)))

#if defined(HYP_FEATURES_BINDLESS_TEXTURES) && !defined(LIGHT_TYPE_AREA_RECT) // area rect light uses DiffuseMap in any case
#define GET_TEXTURE(mat, name) textures[(mat).texture_indices[(MATERIAL_TEXTURE_##name) / 4][(MATERIAL_TEXTURE_##name) % 4]]
#else // !(defined(HYP_FEATURES_BINDLESS_TEXTURES) && !defined(LIGHT_TYPE_AREA_RECT))
#define GET_TEXTURE(mat, name) name
#endif // !(defined(HYP_FEATURES_BINDLESS_TEXTURES) && !defined(LIGHT_TYPE_AREA_RECT))


#if defined(PIXEL_SHADER) || defined(COMPUTE_SHADER)

#define SAMPLE_MATERIAL_TEXTURE(mat, name, texcoord) \
    SAMPLE_TEXTURE_2D(texture_sampler, GET_TEXTURE(mat, name), (texcoord))

#define SAMPLE_MATERIAL_TEXTURE_TRIPLANAR(mat, name, position, normal) \
    SampleTextureTriplanar(texture_sampler, GET_TEXTURE(mat, name), (position), (normal))

#define SAMPLE_MATERIAL_TEXTURE_CUBE(mat, name, texcoord) \
    SAMPLE_TEXTURE_CUBE(texture_sampler, GET_TEXTURE(mat, name), (texcoord))

#else // !(defined(PIXEL_SHADER) || defined(COMPUTE_SHADER))

/// sampling with implicit lod is only allowed in fragment and compute shaders

#define SAMPLE_MATERIAL_TEXTURE(mat, name, texcoord) \
    SAMPLE_TEXTURE_2D_LOD(texture_sampler, GET_TEXTURE(mat, name), (texcoord), 0.0)

#define SAMPLE_MATERIAL_TEXTURE_CUBE(mat, name, texcoord) \
    SAMPLE_TEXTURE_CUBE_LOD(texture_sampler, GET_TEXTURE(mat, name), (texcoord), 0.0)

#endif // !(defined(PIXEL_SHADER) || defined(COMPUTE_SHADER))

#define SAMPLE_MATERIAL_TEXTURE_LOD(mat, name, texcoord, lod) \
    SAMPLE_TEXTURE_2D_LOD(texture_sampler, GET_TEXTURE(mat, name), (texcoord), (lod))

#endif // !HYP_MATERIAL
