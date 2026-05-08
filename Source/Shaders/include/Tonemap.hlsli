#ifndef HYP_TONEMAP
#define HYP_TONEMAP

#include "Shared.hlsli"

#define HDR 1
//#define HDR_TONEMAP_UNCHARTED 1
//#define HDR_TONEMAP_FILMIC 1
//#define HDR_TONEMAP_LOTTES 1
// #define HDR_TONEMAP_UNREAL 1
//#define HDR_TONEMAP_REINHARD 1
#define HDR_TONEMAP_ACES 1

#ifndef EXPOSURE
#define EXPOSURE 1.25
#endif

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
    // x *= EXPOSURE;  // Hardcoded Exposure Adjustment

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
    color *= EXPOSURE;
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

float3 Tonemap(float3 x)
{
#if defined(HDR) && HDR
#if HDR_TONEMAP_FILMIC
    return TonemapFilmic(x);
#elif HDR_TONEMAP_LOTTES
    return TonemapLottes(x);
#elif HDR_TONEMAP_UNREAL
    return TonemapUnreal(x);
#elif HDR_TONEMAP_UNCHARTED
    return TonemapUncharted(x);
#elif HDR_TONEMAP_REINHARD
    return TonemapReinhard(x);
#elif HDR_TONEMAP_ACES
    return TonemapACES(x);
#else
    return TonemapReinhard(x);
#endif
#else
    return x;
#endif
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
