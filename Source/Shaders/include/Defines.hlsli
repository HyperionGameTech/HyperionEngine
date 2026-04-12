#ifndef HYP_SHADER_DEFINES
#define HYP_SHADER_DEFINES

// TEMP: For hlsl intellisense to work
#ifndef HYP_SHADER_COMPILER
    #define LANG_HLSL 1
#endif

#define UINT_TO_VEC4(_value) unpackUnorm4x8((_value))
#define UINT_TO_VEC2(_value) unpackUnorm2x16((_value))
#define VEC4_TO_UINT(_value) packUnorm4x8((_value))
#define VEC2_TO_UINT(_value) packUnorm2x16((_value))

#ifdef LANG_GLSL
    // HLSL Compat
    #define lerp mix
    #define float2 vec2
    #define float3 vec3
    #define float4 vec4
    #define int2 ivec2
    #define int3 ivec3
    #define int4 ivec4
    #define uint2 uvec2
    #define uint3 uvec3
    #define uint4 uvec4
    #define float2x2 mat2
    #define float3x3 mat3
    #define float4x4 mat4
    #define static

    // Sampling

    #define SAMPLE_TEXTURE_2D(samp, tex, texcoord) texture(sampler2D(tex, samp), texcoord)
    #define SAMPLE_TEXTURE_2D_LOD(samp, tex, texcoord, lod) textureLod(sampler2D(tex, samp), texcoord, lod)

    #define SAMPLE_TEXTURE_2D_ARRAY(samp, tex, texcoord) texture(sampler2DArray(tex, samp), texcoord)
    #define SAMPLE_TEXTURE_2D_ARRAY_LOD(samp, tex, texcoord, lod) textureLod(sampler2DArray(tex, samp), texcoord, lod)

    #define SAMPLE_TEXTURE_3D(samp, tex, texcoord) texture(sampler3D(tex, samp), texcoord)
    #define SAMPLE_TEXTURE_3D_LOD(samp, tex, texcoord, lod) textureLod(sampler3D(tex, samp), texcoord, lod)

    #define SAMPLE_TEXTURE_CUBE(samp, tex, texcoord) texture(samplerCube(tex, samp), texcoord)
    #define SAMPLE_TEXTURE_CUBE_LOD(samp, tex, texcoord, lod) textureLod(samplerCube(tex, samp), texcoord, lod)

    #define SAMPLE_TEXTURE_CUBE_ARRAY(samp, tex, texcoord) texture(samplerCubeArray(tex, samp), texcoord)
    #define SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, texcoord, lod) textureLod(samplerCubeArray(tex, samp), texcoord, lod)

    #define TEXEL_FETCH_2D(tex, coord) texelFetch(tex, coord, 0)
    #define TEXEL_FETCH_2D_LOD(samp, tex, coord, lod) texelFetch(sampler2D(tex, samp), coord, lod)

    // Utility

    #define FLOAT_BITS_TO_UINT(value) floatBitsToUint(value)
    #define UINT_BITS_TO_FLOAT(value) uintBitsToFloat(value)

#elif defined(LANG_HLSL)
    #pragma pack_matrix(row_major)

    // For porting GLSL to HLSL
    #define vec2 float2
    #define vec3 float3
    #define vec4 float4
    #define ivec2 int2
    #define ivec3 int3
    #define ivec4 int4
    #define uvec2 uint2
    #define uvec3 uint3
    #define uvec4 uint4
    #define mat3 float3x3
    #define mat4 float4x4
    #define texture2D Texture2D
    #define texture3D Texture3D
    #define textureCube TextureCube
    #define texture2DArray Texture2DArray
    #define textureCubeArray TextureCubeArray
    #define uniform
    #define atan atan2
    #define fract frac
    #define mod fmod
    #define dFdx ddx
    #define dFdy ddy
    #define inversesqrt rsqrt
    #define lessThan(a, b) ((a) < (b))
    #define greaterThan(a, b) ((a) > (b))

    // Sampling

    #define SAMPLE_TEXTURE_2D_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))
#ifdef PIXEL_SHADER
    #define SAMPLE_TEXTURE_2D(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#else
    // we don't have on screen derivatives in other shader stages so just sample LOD0
    #define SAMPLE_TEXTURE_2D(samp, tex, texcoord) (tex).SampleLevel((samp), (texcoord), 0)
#endif

    #define SAMPLE_TEXTURE_2D_ARRAY_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))
#ifdef PIXEL_SHADER
    #define SAMPLE_TEXTURE_2D_ARRAY(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#else
    // we don't have on screen derivatives in other shader stages so just sample LOD0
    #define SAMPLE_TEXTURE_2D_ARRAY(samp, tex, texcoord) (tex).SampleLevel((samp), (texcoord), 0)
#endif

    #define SAMPLE_TEXTURE_3D_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))

#ifdef PIXEL_SHADER
    #define SAMPLE_TEXTURE_3D(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#else
    // we don't have on screen derivatives in other shader stages so just sample LOD0
    #define SAMPLE_TEXTURE_3D(samp, tex, texcoord) (tex).SampleLevel((samp), (texcoord), 0)
#endif

    #define SAMPLE_TEXTURE_CUBE_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))
#ifdef PIXEL_SHADER
    #define SAMPLE_TEXTURE_CUBE(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#else
    // we don't have on screen derivatives in other shader stages so just sample LOD0
    #define SAMPLE_TEXTURE_CUBE(samp, tex, texcoord) (tex).SampleLevel((samp), (texcoord), 0)
#endif

    #define SAMPLE_TEXTURE_CUBE_ARRAY_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))
#ifdef PIXEL_SHADER
    #define SAMPLE_TEXTURE_CUBE_ARRAY(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#else
    // we don't have on screen derivatives in other shader stages so just sample LOD0
    #define SAMPLE_TEXTURE_CUBE_ARRAY(samp, tex, texcoord) (tex).SampleLevel((samp), (texcoord), 0)
#endif

    #define TEXEL_FETCH_2D(tex, coord) (tex).Load(int3((coord), 0))
    #define TEXEL_FETCH_2D_LOD(samp, tex, coord, lod) (tex).Load(int3((coord), (lod)))

    // Utility

    #define FLOAT_BITS_TO_UINT(value) asuint(value)
    #define UINT_BITS_TO_FLOAT(value) asfloat(value)

#endif

#define HYP_MATERIAL_CUBEMAP_TEXTURES 1

#define HYP_MAX_SHADOW_CASCADES 4

#define HYP_MAX_BOUND_AMBIENT_PROBES 4096
#define HYP_MAX_BOUND_REFLECTION_PROBES 16
#define HYP_MAX_BOUND_POINT_SHADOW_MAPS 16
#define HYP_MAX_SHADOW_MAPS 4
#define HYP_MAX_BOUND_ENVIRONMENT_MAPS 1
#define HYP_MAX_MATERIALS 65536

#define HYP_LIGHT_TYPE_DIRECTIONAL 0
#define HYP_LIGHT_TYPE_POINT 1
#define HYP_LIGHT_TYPE_SPOT 2
#define HYP_LIGHT_TYPE_AREA_RECT 3

// Maps to RenderBucket.
#define HYP_OBJECT_BUCKET_OPAQUE 0
#define HYP_OBJECT_BUCKET_LIGHTMAPPED 1
#define HYP_OBJECT_BUCKET_TRANSLUCENT 2
#define HYP_OBJECT_BUCKET_SKY 3
#define HYP_OBJECT_BUCKET_DEBUG 4

// Masking - can use up to 10 bits (stored in R channel of gbuffer normal texture)
#define OBJECT_MASK_OPAQUE (0x01)
#define OBJECT_MASK_LIGHTMAPPED (0x02)
#define OBJECT_MASK_TRANSLUCENT (0x04)
#define OBJECT_MASK_SKY (0x08)
#define OBJECT_MASK_DEBUG (0x10)

// Helper to map bucket to mask.
#define GET_OBJECT_BUCKET_MASK(obj) (1u << (obj).bucket)

// Helper math utilities.
#define HYP_FMATH_SQR(num) ((num) * (num))
#define HYP_FMATH_CUBE(num) ((num) * (num) * (num))
#define HYP_FMATH_PI 3.14159265359
#define HYP_FMATH_PI_HALF 1.57079632679
#define HYP_FMATH_TWO_PI 6.28318530718
#define HYP_FMATH_ONE_OVER_PI 0.31830988618
#define HYP_FMATH_QUARTER_PI 0.7853981625
#define HYP_FMATH_DEG2RAD(deg) ((deg) * HYP_FMATH_PI / 180.0)
#define HYP_FMATH_RAD2DEG(rad) ((rad) * 180.0 / HYP_FMATH_PI)
#define HYP_FMATH_EPSILON 0.00000001
#define HYP_FMATH_INFINITY 99999999.0

#define HYP_FLOAT_MAX 3.402823466e+38F
#define HYP_FLOAT_MIN 1.175494351e-38F

#ifdef __COUNTER__
#define HYP_UNIQUE_NAME(prefix) \
    HYP_CONCAT(prefix, __COUNTER__)
#else
#define HYP_UNIQUE_NAME(prefix) \
    prefix
#endif

#define HYP_FAST_LESS(x, y) (1 - step((y), (x)))

#ifndef HYP_SHADER_COMPILER
    ///// For Intellisense /////

    // Placeholder macro definitions to allow intellisense to work.
    // The engine's shader compiler uses these markers for binding setup
    #define DECLARE_SRV(set, var)
    #define DECLARE_SRV_DYNAMIC(set, var)

    #define DECLARE_UAV(set, var)
    #define DECLARE_UAV_DYNAMIC(set, var)

    #define DECLARE_BUFFER(name, var)
    #define DECLARE_BUFFER_DYNAMIC(name, var)

    #define DECLARE_SAMPLER(set, var)

    // ShaderProperty interfacing
    #define PERMUTE int HYP_UNIQUE_NAME(_permute)
    #define STATIC(name, value) int name = value

    #define HYP_ATTRIBUTE
    #define HYP_ATTRIBUTE_OPTIONAL

    // If we don't set these, we'll have lots of inactive code blocks
    // for HLSL shaders that define multiple shader stages in one file.
    #define VERTEX_SHADER 1
    #define PIXEL_SHADER 1
    #define COMPUTE_SHADER 1
    #define GEOMETRY_SHADER 1
    #define MESH_SHADER 1
    #define TASK_SHADER 1
    #define TESS_CONTROL_SHADER 1
    #define TESS_EVAL_SHADER 1
    #define RAY_GEN_SHADER 1
    #define INTERSECT_SHADER 1
    #define ANY_HIT_SHADER 1
    #define CLOSEST_HIT_SHADER 1
    #define MISS_SHADER 1
#endif // HYP_SHADER_COMPILER

#endif
