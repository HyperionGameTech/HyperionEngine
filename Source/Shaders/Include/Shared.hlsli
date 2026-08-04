#ifndef HYP_SHARED
#define HYP_SHARED

#include "Defines.hlsli"

#ifdef LANG_HLSL

// HLSL <-> GLSL compat
// @TODO: For Shader Model 6.6+ we can use new intrinsics for packing / unpacking
float4 unpackUnorm4x8(uint packedVal)
{
    float4 unpacked;
    unpacked.x = float(packedVal & 0xFF);
    unpacked.y = float((packedVal >> 8) & 0xFF);
    unpacked.z = float((packedVal >> 16) & 0xFF);
    unpacked.w = float((packedVal >> 24) & 0xFF);
    return unpacked / 255.0;
}

float2 unpackUnorm2x16(uint packedVal)
{
    float2 unpacked;
    unpacked.x = float(packedVal & 0xFFFF);
    unpacked.y = float((packedVal >> 16) & 0xFFFF);
    return unpacked / 65535.0;
}

float2 unpackHalf2x16(uint u)
{
    return float2(f16tof32(u & 0xFFFF), f16tof32(u >> 16));
}

float packUnorm4x8(float4 vec)
{
    uint packedVal = uint(vec.x * 255.0) | (uint(vec.y * 255.0) << 8) | (uint(vec.z * 255.0) << 16) | (uint(vec.w * 255.0) << 24);
    return packedVal;
}

float packUnorm2x16(float2 vec)
{
    uint packedVal = uint(vec.x * 65535.0) | (uint(vec.y * 65535.0) << 16);
    return packedVal;
}

uint packHalf2x16(float2 v)
{
    return (f32tof16(v.y) << 16) | f32tof16(v.x);
}

#endif

struct Ray
{
    float3 origin;
    float3 direction;
};

int imod(int a, int b)
{
    return (a % b + b) % b;
}

int2 imod(int2 a, int2 b)
{
    return (a % b + b) % b;
}

int3 imod(int3 a, int3 b)
{
    return (a % b + b) % b;
}

int4 imod(int4 a, int4 b)
{
    return (a % b + b) % b;
}

#define fmod(x, y) ((x) - (y)*floor((x) / (y)))

#define round(x) floor((x) + 0.5)

// #define rcp(x) (1.0 / (x))

float Luminance(float3 color)
{
    return color.r * 0.2125 + color.g * 0.715 + color.b * 0.0721;
}

#define AngleBetweenVectors(a, b) (acos(dot((a), (b))))

float4x4 CreateRotationMatrix(float3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return float4x4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s, 0.0,
        oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
        oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c, 0.0,
        0.0, 0.0, 0.0, 1.0);
}

void ComputeOrthonormalBasis(in float3 normal, out float3 tangent, out float3 bitangent)
{
    float3 T;
    T = cross(normal, float3(0.0, 1.0, 0.0));
    T = lerp(cross(normal, float3(1.0, 0.0, 0.0)), T, step(HYP_FMATH_EPSILON, dot(T, T)));

    T = normalize(T);

    tangent = T;
    bitangent = normalize(cross(normal, T));
}

float3 GetTriplanarBlend(float3 normal)
{
    float3 blending = normalize(max(abs(normal), 0.0001));
    blending /= (blending.x + blending.y + blending.z);

    return blending;
}

#if defined(PIXEL_SHADER) || defined(COMPUTE_SHADER)

#ifdef LANG_GLSL

float4 SampleTextureTriplanar(sampler samp, texture2D tex, float3 position, float3 normal)
{
    float3 blending = GetTriplanarBlend(normal);

    float4 sample_x = SAMPLE_TEXTURE_2D(samp, tex, position.zy * 0.01);
    float4 sample_y = SAMPLE_TEXTURE_2D(samp, tex, position.xz * 0.01);
    float4 sample_z = SAMPLE_TEXTURE_2D(samp, tex, position.xy * 0.01);

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

#endif // PIXEL_SHADER || COMPUTE_SHADER

float4 GaussianBlur9(
    texture2D tex, sampler samp,
    float2 uv,
    float2 direction,
    float radius)
{
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float2 offset1 = float2(1.3846153846, 1.3846153846) * direction;
    float2 offset2 = float2(3.2307692308, 3.2307692308) * direction;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv) * 0.2270270270;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv + (offset1 * radius)) * 0.3162162162;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv - (offset1 * radius)) * 0.3162162162;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv + (offset2 * radius)) * 0.0702702703;
    color += SAMPLE_TEXTURE_2D(samp, tex, uv - (offset2 * radius)) * 0.0702702703;
    return color;
}

float4 GaussianBlur5(
    texture2D tex, sampler samp,
    float2 uv,
    float2 direction,
    float radius)
{
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float2 offset = float2(1.333, 1.333) * direction;
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

float4 ReconstructWorldSpacePositionFromDepth(float4x4 inverse_projection, float4x4 inverse_view, float2 coord, float depth)
{
    float4 ndc = float4(coord.x * 2.0 - 1.0, 1.0 - coord.y * 2.0, depth, 1.0);

    float4 inversed = mul(inverse_projection, ndc);
    inversed /= inversed.w;

    inversed = mul(inverse_view, inversed);

    return inversed;
}

float4 ReconstructViewSpacePositionFromDepth(float4x4 inverse_projection, float2 coord, float depth)
{
    float4 ndc = float4(coord.x * 2.0 - 1.0, 1.0 - coord.y * 2.0, depth, 1.0);

    float4 inversed = mul(inverse_projection, ndc);
    inversed /= inversed.w;

    return inversed;
}

float2 GetProjectedPositionFromView(in float4x4 projection, in float3 view_space_position)
{
    float4 sample_position = mul(projection, float4(view_space_position, 1.0));
    sample_position /= sample_position.w;
    sample_position.x = sample_position.x * 0.5 + 0.5;
    sample_position.y = (-sample_position.y) * 0.5 + 0.5;
    return sample_position.xy;
}

float3x3 inverse(float3x3 m)
{
    float3 a = m[0];
    float3 b = m[1];
    float3 c = m[2];

    float3 r0 = cross(b, c);
    float3 r1 = cross(c, a);
    float3 r2 = cross(a, b);

    float det = dot(r2, c);
    float invDet = 1.0 / max(abs(det), 1e-8);

    return float3x3(r0 * invDet, r1 * invDet, r2 * invDet);
}

float4 CalculateFogExp(in float4 start_color, in float4 end_color, float3 world_position, float3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);
    const float density = 0.00003;

    const float fog_factor = 1.0 / exp(dist * density);

    return lerp(start_color, end_color, 1.0 - fog_factor);
}

float4 CalculateFogLinear(in float4 start_color, in float4 end_color, float3 world_position, float3 camera_position, float fog_start, float fog_end)
{
    const float dist = distance(world_position, camera_position);

    const float fog_factor = saturate((fog_end - dist) / (fog_end - fog_start));

    return lerp(start_color, end_color, 1.0 - fog_factor);
}

float RoughnessToConeAngle(float roughness)
{
    //roughness = clamp(roughness, 1e-3f, 1.0f);
    //const float alpha = roughness * roughness;
    //const float specPower = 2.0 / (alpha * alpha) - 2.0;
    //static const float s_threshold = 0.244;
    //const float cosAngle = pow(s_threshold, 1.0 / (specPower + 1.0));
    //const float clampedCos = clamp(cosAngle, -1.0, 1.0);
    //return acos(clampedCos);

    float specular_power = 2.0 / pow(roughness, 4.0) - 2.0;

    const float xi = 0.244;
    float exponent = 1.0 / (specular_power + 1.0);
    return acos(pow(xi, exponent));
}

float linstep(float min, float max, float v)
{
    return clamp((v - min) / (max - min), 0.0, 1.0);
}

float3 RGBToYCoCg(in float3 rgb)
{
    float co = rgb.r - rgb.b;
    float t = rgb.b + co / 2.0;
    float cg = rgb.g - t;
    float y = t + cg / 2.0;
    return float3(y, co, cg);
}

float4 RGBToYCoCg(in float4 rgb)
{
    float co = rgb.r - rgb.b;
    float t = rgb.b + co / 2.0;
    float cg = rgb.g - t;
    float y = t + cg / 2.0;
    return float4(y, co, cg, rgb.a);
}

float3 YCoCgToRGB(in float3 ycocg)
{
    float t = ycocg.r - ycocg.b / 2.0;
    float g = ycocg.b + t;
    float b = t - ycocg.g / 2.0;
    float r = ycocg.g + b;
    return float3(r, g, b);
}

float4 YCoCgToRGB(in float4 ycocg)
{
    float t = ycocg.r - ycocg.b / 2.0;
    float g = ycocg.b + t;
    float b = t - ycocg.g / 2.0;
    float r = ycocg.g + b;
    return float4(r, g, b, ycocg.a);
}

//// Cubemap utilities

// pairs of cubemap forward direction and up direction (interleaved order)
static const float3 g_cubemapDirections[12] = {
    float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
    float3(-1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
    float3(0.0, 1.0, 0.0), float3(0.0, 0.0, -1.0),
    float3(0.0, -1.0, 0.0), float3(0.0, 0.0, 1.0),
    float3(0.0, 0.0, 1.0), float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, -1.0), float3(0.0, 1.0, 0.0)
};

float3 GetCubemapCoord(uint face, float2 uv)
{
    float3 forward = g_cubemapDirections[face * 2];
    float3 up = g_cubemapDirections[face * 2 + 1];
    float3 right = cross(forward, up);

    float2 coord = uv * 2.0 - 1.0;

    return normalize(forward - right * coord.x - up * coord.y);
}

uint GetCubemapFaceIndex(float3 dir)
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
