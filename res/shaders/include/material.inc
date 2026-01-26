#ifndef HYP_MATERIAL_GLSL
#define HYP_MATERIAL_GLSL

#include "defines.inc"
#include "shared.inc"

// define textures

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    HYP_DESCRIPTOR_SRV(GlobalBindless, Textures) uniform texture2D textures[];
#else
    #if HAS_ALBEDO_MAP
        HYP_DESCRIPTOR_SRV(Material, AlbedoMap) uniform texture2D AlbedoMap;
    #endif
    #if HAS_NORMAL_MAP
        HYP_DESCRIPTOR_SRV(Material, NormalMap) uniform texture2D NormalMap;
    #endif
    #if HAS_PARALLAX_MAP
        HYP_DESCRIPTOR_SRV(Material, ParallaxMap) uniform texture2D ParallaxMap;
    #endif
    #if HAS_METALNESS_MAP
        HYP_DESCRIPTOR_SRV(Material, MetalnessMap) uniform texture2D MetalnessMap;
    #endif
    #if HAS_ROUGHNESS_MAP
        HYP_DESCRIPTOR_SRV(Material, RoughnessMap) uniform texture2D RoughnessMap;
    #endif
    #if HAS_AO_MAP
        HYP_DESCRIPTOR_SRV(Material, AoMap) uniform texture2D AoMap;
    #endif
#endif

#endif

struct Material
{
    vec4 albedo;

    uvec4 packed_params;

    vec2 uv_scale;
    float parallax_height;
    uint textureUsage;

    uvec4 texture_indices[4]; // moltenvk is compiling uint32_t texture_index[16] -> uint4 texture_index[16]
};

// enum for packed params
#define MATERIAL_PARAM_ROUGHNESS 0
#define MATERIAL_PARAM_METALNESS 1
#define MATERIAL_PARAM_TRANSMISSION 2
#define MATERIAL_PARAM_NORMAL_MAP_INTENSITY 3
#define MATERIAL_PARAM_ALPHA_THRESHOLD 4
#define MATERIAL_PARAM_UI_BACKGROUND_COLOR 15

#define GET_MATERIAL_PARAM(mat, index) \
    UINT_TO_VEC4(mat.packed_params[(index) / 4])[(index) % 4]

#define MATERIAL_TEXTURE_AlbedoMap 0
#define MATERIAL_TEXTURE_NormalMap 1
#define MATERIAL_TEXTURE_ParallaxMap 2
#define MATERIAL_TEXTURE_MetalnessMap 3
#define MATERIAL_TEXTURE_RoughnessMap 4
#define MATERIAL_TEXTURE_AoMap 5

#define MATERIAL_ALPHA_DISCARD 0.25

#define HAS_TEXTURE(mat, name) bool(((mat).textureUsage & (1 << MATERIAL_TEXTURE_##name)))

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
#define GET_TEXTURE(mat, name) textures[(mat).texture_indices[(MATERIAL_TEXTURE_##name) / 4][(MATERIAL_TEXTURE_##name) % 4]]
#else
#define GET_TEXTURE(mat, name) name
#endif

#define SAMPLE_MATERIAL_TEXTURE(mat, name, texcoord) \
    SAMPLE_TEXTURE_2D(texture_sampler, GET_TEXTURE(mat, name), (texcoord))

#define SAMPLE_MATERIAL_TEXTURE_LOD(mat, name, texcoord, lod) \
    SAMPLE_TEXTURE_2D_LOD(texture_sampler, GET_TEXTURE(mat, name), (texcoord), (lod))

#define SAMPLE_MATERIAL_TEXTURE_TRIPLANAR(mat, name, position, normal) \
    SampleTextureTriplanar(texture_sampler, GET_TEXTURE(mat, name), (position), (normal))

#define SAMPLE_MATERIAL_TEXTURE_CUBE(mat, name, texcoord) \
    SAMPLE_TEXTURE_CUBE(texture_sampler, GET_TEXTURE(mat, name), (texcoord))

#endif