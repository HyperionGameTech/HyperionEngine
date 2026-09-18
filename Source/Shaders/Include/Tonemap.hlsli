#ifndef HYP_TONEMAP
#define HYP_TONEMAP

#include "Shared.hlsli"
#include "Scene.hlsli"

// matches TonemapOperator in Scene/EnvironmentSettings.hpp
#define TONEMAP_OPERATOR_AGX 0
#define TONEMAP_OPERATOR_AGX_PUNCHY 1
#define TONEMAP_OPERATOR_ACES 2
#define TONEMAP_OPERATOR_PBR_NEUTRAL 3
#define TONEMAP_OPERATOR_REINHARD 4

// Source for some of these: https://dmnsgn.github.io/glsl-tone-map

float3 _TonemapUncharted(float3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 TonemapUncharted(float3 color)
{
    const float W = 11.2;
    float exposureBias = 2.0;
    float3 curr = _TonemapUncharted(exposureBias * color);
    float3 whiteScale = 1.0 / _TonemapUncharted((float3)W);
    return curr * whiteScale;
}

float3 _TonemapFilmic(float3 x)
{
    x = 1.0 - exp(-1.0 * x);

    float3 X = max((float3)0.0, x - 0.004);
    return (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
}

float3 TonemapFilmic(float3 x)
{
    // const float ExposureBias = 2.0;
    // x *= ExposureBias;

    // float3 whiteScale = 1.0 / _TonemapFilmic(W.xxx);
    // return x * whiteScale;

    return _TonemapFilmic(x);
}

// Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
float3 TonemapLottes(float3 x)
{
    const float3 a = (float3)1.6;
    const float3 d = (float3)0.977;
    const float3 hdrMax = (float3)8.0;
    const float3 midIn = (float3)0.18;
    const float3 midOut = (float3)0.267;

    const float3 b = (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    const float3 c = (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(x, a) / (pow(x, a * d) * b + c);
}

float3 TonemapUnreal(float3 x)
{
    return x / (x + 0.155) * 1.019;
}

float GetLuminance(float3 x)
{
    return dot(x, float3(0.2126, 0.7152, 0.0722));
}

float3 ChangeLuminance(float3 x, float new_luminance)
{
    float luminance = GetLuminance(x);

    return x * (new_luminance / max(luminance, 0.00001));
}

float3 TonemapReinhard(float3 x)
{
    const float max_white_l = 100.0;
    float l_old = GetLuminance(x);
    float numerator = l_old * (1.0f + (l_old / (max_white_l * max_white_l)));
    float l_new = numerator / max(1.0f + l_old, 0.00001);
    return ChangeLuminance(x, l_new);
}

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat = float3x3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat = float3x3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

float3 _RRTAndODTFit(float3 color)
{
    float3 a = color * (color + 0.0245786) - 0.000090537;
    float3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    return a / b;
}

// see https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/Renderer/shaders/tonemapping.glsl
float3 TonemapACES(float3 color)
{
    color /= 0.6;
#ifdef DX12
    // DX12 matrix layout with row_major packing results in transposed matrices
    // compared to Vulkan. Using reversed multiplication order to compensate.
    color = mul(color, ACESInputMat);
    color = _RRTAndODTFit(color);
    color = mul(color, ACESOutputMat);
#else
    color = mul(ACESInputMat, color);
    color = _RRTAndODTFit(color);
    color = mul(ACESOutputMat, color);
#endif
    return clamp(color, 0.0, 1.0);
}

float3 TonemapReinhardSimple(float3 x)
{
    return x / (1.0 + x);
}

float3 ReverseTonemapReinhardSimple(float3 x)
{
    return x / (1.0 - x);
}

// Minimal AgX fit by Benjamin Wrensch, see https://iolite-engine.com/blog_posts/minimal_agx_implementation
float3 AgXDefaultContrastApproximation(float3 x)
{
    const float3 x2 = x * x;
    const float3 x4 = x2 * x2;

    return 15.5 * x4 * x2
        - 40.14 * x4 * x
        + 31.96 * x4
        - 6.868 * x2 * x
        + 0.4298 * x2
        + 0.1191 * x
        - 0.00232;
}

float3 TonemapAgX(float3 color, bool punchy)
{
    static const float MinEv = -12.47393;
    static const float MaxEv = 4.026069;

    color = float3(
        dot(float3(0.842479062253094, 0.0784335999999992, 0.0792237451477643), color),
        dot(float3(0.0423282422610123, 0.878468636469772, 0.0791661274605434), color),
        dot(float3(0.0423756549057051, 0.0784336, 0.879142973793104), color));

    color = clamp(log2(max(color, (float3)1e-10)), MinEv, MaxEv);
    color = (color - MinEv) / (MaxEv - MinEv);
    color = AgXDefaultContrastApproximation(color);

    if (punchy)
    {
        const float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));

        color = pow(max(color, (float3)0.0), (float3)1.35);
        color = luminance + 1.4 * (color - luminance);
    }

    color = float3(
        dot(float3(1.19687900512017, -0.0980208811401368, -0.0990297440797205), color),
        dot(float3(-0.0528968517574562, 1.15190312990417, -0.0989611768448433), color),
        dot(float3(-0.0529716355144438, -0.0980434501171241, 1.15107367264116), color));

    // back to linear for the sRGB swapchain
    return pow(max(color, (float3)0.0), (float3)2.2);
}

// Khronos PBR Neutral, see https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral
float3 TonemapPBRNeutral(float3 color)
{
    static const float StartCompression = 0.8 - 0.04;
    static const float Desaturation = 0.15;

    const float minChannel = min(color.r, min(color.g, color.b));
    const float offset = minChannel < 0.08 ? minChannel - 6.25 * minChannel * minChannel : 0.04;
    color -= offset;

    const float peak = max(color.r, max(color.g, color.b));

    if (peak < StartCompression)
    {
        return color;
    }

    const float compressionRange = 1.0 - StartCompression;
    const float newPeak = 1.0 - compressionRange * compressionRange / (peak + compressionRange - StartCompression);
    color *= newPeak / peak;

    const float desaturationAmount = 1.0 - 1.0 / (Desaturation * (peak - newPeak) + 1.0);

    return lerp(color, (float3)newPeak, desaturationAmount);
}

// exposure, white balance, then contrast around mid grey and saturation, all in scene linear before the tonemapper
float3 ApplyColorGrading(float3 color, WorldShaderData worldShaderData)
{
    static const float MidGreyLog = -2.4739312; // log2(0.18)

    color *= worldShaderData.exposure_grading.x;

    color = float3(
        dot(worldShaderData.white_balance_rows[0].xyz, color),
        dot(worldShaderData.white_balance_rows[1].xyz, color),
        dot(worldShaderData.white_balance_rows[2].xyz, color));

    color = max(color, (float3)0.0);

    color = pow((float3)2.0, (log2(max(color, (float3)1e-6)) - MidGreyLog) * worldShaderData.exposure_grading.y + MidGreyLog);

    const float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));

    return max(lerp((float3)luminance, color, worldShaderData.exposure_grading.z), (float3)0.0);
}

float3 Tonemap(float3 color, uint tonemapOperator)
{
    switch (tonemapOperator)
    {
    case TONEMAP_OPERATOR_AGX_PUNCHY:
        return TonemapAgX(color, true);
    case TONEMAP_OPERATOR_ACES:
        return TonemapACES(color);
    case TONEMAP_OPERATOR_PBR_NEUTRAL:
        return TonemapPBRNeutral(color);
    case TONEMAP_OPERATOR_REINHARD:
        return TonemapReinhard(color);
    default:
        return TonemapAgX(color, false);
    }
}

static const float ST2084_m1 = 2610.0 / 16384.0;
static const float ST2084_m2 = 2523.0 / 32.0;
static const float ST2084_c1 = 3424.0 / 4096.0;
static const float ST2084_c2 = 2413.0 / 128.0;
static const float ST2084_c3 = 2392.0 / 128.0;

float3 LinearToPQ(float3 color, float peakNits)
{
    // Map scene linear HDR (0..peakNits) to [0..1] PQ
    float3 L = clamp(color / peakNits, 0.0, 1.0) * 10000.0; // normalize to 10000 nits max
    float3 num = ST2084_c1.xxx + ST2084_c2.xxx * pow(L, ST2084_m1.xxx);
    float3 denom = 1.0 + ST2084_c3.xxx * pow(L, ST2084_m1.xxx);
    return pow(num / denom, ST2084_m2.xxx);
}

#endif
