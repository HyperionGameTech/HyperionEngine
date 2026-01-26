#ifndef HYP_SHARED_GLSL
#define HYP_SHARED_GLSL

#include "defines.inc"

#ifdef LANG_GLSL
// HLSL Compat
#define lerp mix
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4
#else
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
#endif

#define HYP_FAST_LESS(x, y) (1 - step((y), (x)))

struct Ray
{
    vec3 origin;
    vec3 direction;
};

int imod(int a, int b)
{
    return (a % b + b) % b;
}

ivec2 imod(ivec2 a, ivec2 b)
{
    return (a % b + b) % b;
}

ivec3 imod(ivec3 a, ivec3 b)
{
    return (a % b + b) % b;
}

ivec4 imod(ivec4 a, ivec4 b)
{
    return (a % b + b) % b;
}

#define fmod(x, y) ((x) - (y)*floor((x) / (y)))

#define round(x) floor((x) + 0.5)

// #define rcp(x) (1.0 / (x))

float Luminance(vec3 color)
{
    return color.r * 0.2125 + color.g * 0.715 + color.b * 0.0721;
}

#define AngleBetweenVectors(a, b) (acos(dot((a), (b))))

mat4 CreateRotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s, 0.0,
        oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
        oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c, 0.0,
        0.0, 0.0, 0.0, 1.0);
}

void ComputeOrthonormalBasis(in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    vec3 T;
    T = cross(normal, vec3(0.0, 1.0, 0.0));
    T = lerp(cross(normal, vec3(1.0, 0.0, 0.0)), T, step(HYP_FMATH_EPSILON, dot(T, T)));

    T = normalize(T);

    tangent = T;
    bitangent = normalize(cross(normal, T));
}

#ifdef LANG_GLSL

#if HYP_GBUFFER_FLIP_Y
#define SAMPLE_TEXTURE_2D(samp, tex, texcoord) texture(sampler2D(tex, samp), vec2((texcoord).s, 1.0 - (texcoord).t))
#define SAMPLE_TEXTURE_2D_LOD(samp, tex, texcoord, lod) textureLod(sampler2D(tex, samp), vec2((texcoord).s, 1.0 - (texcoord).t), lod)
#else // !HYP_GBUFFER_FLIP_Y
#define SAMPLE_TEXTURE_2D(samp, tex, texcoord) texture(sampler2D(tex, samp), texcoord)
#define SAMPLE_TEXTURE_2D_LOD(samp, tex, texcoord, lod) textureLod(sampler2D(tex, samp), texcoord, lod)
#endif

#define TEXEL_FETCH_2D(tex, coord) texelFetch(tex, coord, 0)
#define TEXEL_FETCH_2D_LOD(samp, tex, coord, lod) texelFetch(sampler2D(tex, samp), coord, lod)

#define SAMPLE_TEXTURE_3D(samp, tex, texcoord) texture(sampler3D(tex, samp), texcoord)
#define SAMPLE_TEXTURE_3D_LOD(samp, tex, texcoord, lod) textureLod(sampler3D(tex, samp), texcoord, lod)

#define SAMPLE_TEXTURE_CUBE(samp, tex, texcoord) texture(samplerCube(tex, samp), texcoord)
#define SAMPLE_TEXTURE_CUBE_LOD(samp, tex, texcoord, lod) textureLod(samplerCube(tex, samp), texcoord, lod)

#elif defined(LANG_HLSL)

#if HYP_GBUFFER_FLIP_Y
#define SAMPLE_TEXTURE_2D(samp, tex, texcoord) (tex).Sample((samp), float2((texcoord).x, 1.0 - (texcoord).y))
#define SAMPLE_TEXTURE_2D_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), float2((texcoord).x, 1.0 - (texcoord).y), (lod))
#else // !HYP_GBUFFER_FLIP_Y
#define SAMPLE_TEXTURE_2D(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#define SAMPLE_TEXTURE_2D_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))
#endif

#define TEXEL_FETCH_2D(tex, coord) (tex).Load(int3((coord), 0))
#define TEXEL_FETCH_2D_LOD(samp, tex, coord, lod) (tex).Load(int3((coord), 0), (lod))

#define SAMPLE_TEXTURE_3D(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#define SAMPLE_TEXTURE_3D_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))

#define SAMPLE_TEXTURE_CUBE(samp, tex, texcoord) (tex).Sample((samp), (texcoord))
#define SAMPLE_TEXTURE_CUBE_LOD(samp, tex, texcoord, lod) (tex).SampleLevel((samp), (texcoord), (lod))

#else

#error "No LANG_* preprocessor define provided"

#endif // !LANG_HLSL

vec3 GetTriplanarBlend(vec3 normal)
{
    vec3 blending = normalize(max(abs(normal), 0.0001));
    blending /= (blending.x + blending.y + blending.z);

    return blending;
}

#ifdef LANG_GLSL

vec4 SampleTextureTriplanar(sampler samp, texture2D tex, vec3 position, vec3 normal)
{
    vec3 blending = GetTriplanarBlend(normal);

    vec4 sample_x = SAMPLE_TEXTURE_2D(samp, tex, position.zy * 0.01);
    vec4 sample_y = SAMPLE_TEXTURE_2D(samp, tex, position.xz * 0.01);
    vec4 sample_z = SAMPLE_TEXTURE_2D(samp, tex, position.xy * 0.01);

    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

#elif defined(LANG_HLSL)

float4 SampleTextureTriplanar(sampler samp, Texture2D tex, float3 position, float3 normal)
{
    float3 blending = GetTriplanarBlend(normal);

    float4 sample_x = SAMPLE_TEXTURE_2D(samp, tex, position.zy * 0.01);
    float4 sample_y = SAMPLE_TEXTURE_2D(samp, tex, position.xz * 0.01);
    float4 sample_z = SAMPLE_TEXTURE_2D(samp, tex, position.xy * 0.01);

    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

#endif

#define GAUSS_TABLE_SIZE 15

#ifdef LANG_GLSL
const float gauss_table[GAUSS_TABLE_SIZE + 1] = float[](
    0.1847392078702266,
    0.16595854345772326,
    0.12031364177766891,
    0.07038755277896766,
    0.03322925565155569,
    0.012657819729901945,
    0.0038903040680094217,
    0.0009646503390864025,
    0.00019297087402915717,
    0.000031139936308099136,
    0.000004053309048174758,
    4.255228059965837e-7,
    3.602517634249573e-8,
    2.4592560765896795e-9,
    1.3534945386863618e-10,
    0.0);
#elif defined(LANG_HLSL)
static const float gauss_table[GAUSS_TABLE_SIZE + 1] = {
    0.1847392078702266,
    0.16595854345772326,
    0.12031364177766891,
    0.07038755277896766,
    0.03322925565155569,
    0.012657819729901945,
    0.0038903040680094217,
    0.0009646503390864025,
    0.00019297087402915717,
    0.000031139936308099136,
    0.000004053309048174758,
    4.255228059965837e-7,
    3.602517634249573e-8,
    2.4592560765896795e-9,
    1.3534945386863618e-10,
    0.0 };
#else
#error "No LANG_* preprocessor define provided"
#endif

float GaussianWeight(float value)
{
    float idxf;
    float c = modf(max(0.0, value * float(GAUSS_TABLE_SIZE)), idxf);
    int idx = int(idxf);
    if (idx >= GAUSS_TABLE_SIZE + 1)
    {
        return 0.0;
    }

    return lerp(gauss_table[idx], gauss_table[idx + 1], c);
}

vec4 GaussianBlur9(
    texture2D tex, sampler samp,
    vec2 uv,
    vec2 direction,
    float radius)
{
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 offset1 = vec2(1.3846153846, 1.3846153846) * direction;
    vec2 offset2 = vec2(3.2307692308, 3.2307692308) * direction;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv) * 0.2270270270;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv + (offset1 * radius)) * 0.3162162162;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv - (offset1 * radius)) * 0.3162162162;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv + (offset2 * radius)) * 0.0702702703;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv - (offset2 * radius)) * 0.0702702703;
    return color;
}

vec4 GaussianBlur5(
    texture2D tex, sampler samp,
    vec2 uv,
    vec2 direction,
    float radius)
{
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 offset = vec2(1.333, 1.333) * direction;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv) * 0.29411764705882354;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv + (offset * radius)) * 0.35294117647058826;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv - (offset * radius)) * 0.35294117647058826;
    return color;
}

// Based on Unity's Linear01Depth function, Unity uses a reversed depth buffer though.
float Linear01Depth(float depth, float near, float far)
{
    float x = 1.0 - (far / near);
    float y = (far / near);
    float z = x / far;
    float w = y / far;

    return 1.0 - (1.0 / (x * (1.0 - depth) + y));
}

float ViewDepth(float depth, float near, float far)
{
    float x = 1.0 - (far / near);
    float y = (far / near);
    float z = x / far;
    float w = y / far;

    return 1.0 / (z * depth + w);

    // return (far * near) / (far - depth * (far - near));
}

vec4 ReconstructWorldSpacePositionFromDepth(mat4 inverse_projection, mat4 inverse_view, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

#ifdef LANG_HLSL
    vec4 inversed = mul(inverse_projection, ndc);
#else
    vec4 inversed = inverse_projection * ndc;
#endif
    inversed /= inversed.w;

#ifdef LANG_HLSL
    inversed = mul(inverse_view, inversed);
#else
    inversed = inverse_view * inversed;
#endif

    return inversed;
}

vec4 ReconstructViewSpacePositionFromDepth(mat4 inverse_projection, vec2 coord, float depth)
{
    vec4 ndc = vec4(coord * 2.0 - 1.0, depth, 1.0);

#ifdef LANG_HLSL
    vec4 inversed = mul(inverse_projection, ndc);
#else
    vec4 inversed = inverse_projection * ndc;
#endif
    inversed /= inversed.w;

    return inversed;
}

vec2 GetProjectedPositionFromWorld(in mat4 projection, in mat4 view, in vec3 world_space_position)
{
#ifdef LANG_HLSL
    vec4 sample_position = mul(projection, mul(view, vec4(world_space_position, 1.0)));
#else
    vec4 sample_position = projection * view * vec4(world_space_position, 1.0);
#endif
    return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

vec2 GetProjectedPositionFromWorld(in mat4 view_projection, in vec3 world_space_position)
{
#ifdef LANG_HLSL
    vec4 sample_position = mul(view_projection, vec4(world_space_position, 1.0));
#else
    vec4 sample_position = view_projection * vec4(world_space_position, 1.0);
#endif
    return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

vec2 GetProjectedPositionFromView(in mat4 projection, in vec3 view_space_position)
{
#ifdef LANG_HLSL
    vec4 sample_position = mul(projection, vec4(view_space_position, 1.0));
#else
    vec4 sample_position = projection * vec4(view_space_position, 1.0);
#endif
    return (sample_position.xy / sample_position.w) * 0.5 + 0.5;
}

#ifndef LANG_HLSL
float saturate(float val)
{
    return min(max(val, 0.0), 1.0);
}

vec2 saturate(vec2 val)
{
    return min(max(val, vec2(0.0)), vec2(1.0));
}

vec3 saturate(vec3 val)
{
    return min(max(val, vec3(0.0)), vec3(1.0));
}

vec4 saturate(vec4 val)
{
    return min(max(val, vec4(0.0)), vec4(1.0));
}
#endif

vec4 CalculateFogExp(in vec4 start_color, in vec4 end_color, vec3 world_position, vec3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);
    const float density = 0.00003;

    const float fog_factor = 1.0 / exp(dist * density);

    return lerp(start_color, end_color, 1.0 - fog_factor);
}

vec4 CalculateFogLinear(in vec4 start_color, in vec4 end_color, vec3 world_position, vec3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);

    const float fog_factor = saturate((fog_end - dist) / (fog_end - fog_start));

    return lerp(start_color, end_color, 1.0 - fog_factor);
}

float RoughnessToConeAngle(float roughness)
{
    float specular_power = 2.0 / pow(roughness, 4.0) - 2.0;

    const float xi = 0.244;
    float exponent = 1.0 / (specular_power + 1.0);
    return acos(pow(xi, exponent));
}

float linstep(float min, float max, float v)
{
    return clamp((v - min) / (max - min), 0.0, 1.0);
}

vec3 RGBToYCoCg(in vec3 rgb)
{
    float co = rgb.r - rgb.b;
    float t = rgb.b + co / 2.0;
    float cg = rgb.g - t;
    float y = t + cg / 2.0;
    return vec3(y, co, cg);
}

vec4 RGBToYCoCg(in vec4 rgb)
{
    float co = rgb.r - rgb.b;
    float t = rgb.b + co / 2.0;
    float cg = rgb.g - t;
    float y = t + cg / 2.0;
    return vec4(y, co, cg, rgb.a);
}

vec3 YCoCgToRGB(in vec3 ycocg)
{
    float t = ycocg.r - ycocg.b / 2.0;
    float g = ycocg.b + t;
    float b = t - ycocg.g / 2.0;
    float r = ycocg.g + b;
    return vec3(r, g, b);
}

vec4 YCoCgToRGB(in vec4 ycocg)
{
    float t = ycocg.r - ycocg.b / 2.0;
    float g = ycocg.b + t;
    float b = t - ycocg.g / 2.0;
    float r = ycocg.g + b;
    return vec4(r, g, b, ycocg.a);
}

//// Cubemap utilities

#ifdef LANG_GLSL
// pairs of cubemap forward direction and up direction (interleaved order)
const vec3 g_cubemapDirections[12] = vec3[](
    vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, -1.0),
    vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, -1.0), vec3(0.0, 1.0, 0.0));
#elif defined(LANG_HLSL)
// pairs of cubemap forward direction and up direction (interleaved order)
static const float3 g_cubemapDirections[12] = {
    float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
    float3(-1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
    float3(0.0, 1.0, 0.0), float3(0.0, 0.0, -1.0),
    float3(0.0, -1.0, 0.0), float3(0.0, 0.0, 1.0),
    float3(0.0, 0.0, 1.0), float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, -1.0), float3(0.0, 1.0, 0.0)
};
#endif

vec3 GetCubemapCoord(uint face, vec2 uv)
{
    vec3 forward = g_cubemapDirections[face * 2];
    vec3 up = g_cubemapDirections[face * 2 + 1];
    vec3 right = cross(forward, up);

    vec2 coord = uv * 2.0 - 1.0;

    return normalize(forward - right * coord.x - up * coord.y);
}

uint GetCubemapFaceIndex(vec3 dir)
{
    uint face = 0u;
    float ax = abs(dir.x);
    float ay = abs(dir.y);
    float az = abs(dir.z);

    if (ay > ax && ay > az)
    {
        face = (dir.y < 0.0) ? 3u : 2u;
    }
    else if (az > ax && az > ay)
    {
        face = (dir.z < 0.0) ? 5u : 4u;
    }
    else
    {
        face = (dir.x < 0.0) ? 1u : 0u;
    }

    return face;
}

///// End cubemap utilities

#endif