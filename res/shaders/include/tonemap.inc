#ifndef HYP_TONEMAP_GLSL
#define HYP_TONEMAP_GLSL

#include "shared.inc"

#define HDR 1
//#define HDR_TONEMAP_UNCHARTED 1
#define HDR_TONEMAP_FILMIC 1
// #define HDR_TONEMAP_UNREAL 1
//#define HDR_TONEMAP_REINHARD 1

#ifndef HDR_EXPOSURE
#define HDR_EXPOSURE 9.0
#endif

static const float A = 0.15;
static const float B = 0.50;
static const float C = 0.10;
static const float D = 0.20;
static const float E = 0.02;
static const float F = 0.30;
static const float W = 11.2;

float3 _TonemapUncharted(float3 x)
{
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 TonemapUncharted(float3 x)
{
    x *= HDR_EXPOSURE; // Hardcoded Exposure Adjustment

    const float ExposureBias = 2.0;
    float3 curr = _TonemapUncharted(x * ExposureBias);

    float3 whiteScale = 1.0 / _TonemapUncharted(W.xxx);
    return curr * whiteScale;
}
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat = float3x3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat = float3x3(
    1.60475, -0.53108, -0.07367,
    -0.10208, 1.10813, -0.00605,
    -0.00327, -0.07276, 1.07602);

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 TonemapACESFilmic(float3 color)
{
    color = mul(color, ACESInputMat);

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = mul(color, ACESOutputMat);

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

float3 _TonemapFilmic(float3 x)
{
    x = 1.0 - exp(-1.0 * x);

    float3 X = max((float3)0.0, x - 0.004);
    return (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
}

float3 TonemapFilmic(float3 x)
{
    // x *= HDR_EXPOSURE;  // Hardcoded Exposure Adjustment

    // const float ExposureBias = 2.0;
    // x *= ExposureBias;

    // float3 whiteScale = 1.0 / _TonemapFilmic(W.xxx);
    // return x * whiteScale;

    return _TonemapFilmic(x);
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
    return TonemapFilmic(x); // TonemapACESFilmic(x);
#elif HDR_TONEMAP_UNREAL
    return TonemapUnreal(x);
#elif HDR_TONEMAP_UNCHARTED
    return TonemapUncharted(x);
#elif HDR_TONEMAP_REINHARD
    return TonemapReinhard(x);
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