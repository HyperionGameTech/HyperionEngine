#ifndef HYP_TONEMAP_GLSL
#define HYP_TONEMAP_GLSL

#include "shared.inc"

#define HDR 1
// #define HDR_TONEMAP_UNCHARTED 1
//#define HDR_TONEMAP_FILMIC 1
// #define HDR_TONEMAP_UNREAL 1
#define HDR_TONEMAP_REINHARD 1

#ifndef HDR_EXPOSURE
#define HDR_EXPOSURE 9.0
#endif

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

vec3 _TonemapUncharted(vec3 x)
{
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 TonemapUncharted(vec3 x)
{
    x *= HDR_EXPOSURE; // Hardcoded Exposure Adjustment

    const float ExposureBias = 2.0;
    vec3 curr = _TonemapUncharted(x * ExposureBias);

    vec3 whiteScale = 1.0 / _TonemapUncharted(vec3(W));
    return curr * whiteScale;
}
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
    1.60475, -0.53108, -0.07367,
    -0.10208, 1.10813, -0.00605,
    -0.00327, -0.07276, 1.07602);

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 TonemapACESFilmic(vec3 color)
{
    color = color * ACESInputMat;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = color * ACESOutputMat;

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

vec3 _TonemapFilmic(vec3 x)
{
    x = 1.0 - exp(-1.0 * x);

    vec3 X = max(vec3(0.0), x - 0.004);
    return (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
}

vec3 TonemapFilmic(vec3 x)
{
    // x *= HDR_EXPOSURE;  // Hardcoded Exposure Adjustment

    // const float ExposureBias = 2.0;
    // x *= ExposureBias;

    // vec3 whiteScale = 1.0 / _TonemapFilmic(vec3(W));
    // return x * whiteScale;

    return _TonemapFilmic(x);
}

vec3 TonemapUnreal(vec3 x)
{
    return x / (x + 0.155) * 1.019;
}

float GetLuminance(vec3 x)
{
    return dot(x, vec3(0.2126, 0.7152, 0.0722));
}

vec3 ChangeLuminance(vec3 x, float new_luminance)
{
    float luminance = GetLuminance(x);

    return x * (new_luminance / max(luminance, 0.00001));
}

vec3 TonemapReinhard(vec3 x)
{
    const float max_white_l = 100.0;
    float l_old = GetLuminance(x);
    float numerator = l_old * (1.0f + (l_old / (max_white_l * max_white_l)));
    float l_new = numerator / max(1.0f + l_old, 0.00001);
    return ChangeLuminance(x, l_new);
}

vec3 TonemapReinhardSimple(vec3 x)
{
    return x / (vec3(1.0) + x);
}

vec3 ReverseTonemapReinhardSimple(vec3 x)
{
    return x / (vec3(1.0) - x);
}

vec3 Tonemap(vec3 x)
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

const float ST2084_m1 = 2610.0 / 16384.0;
const float ST2084_m2 = 2523.0 / 32.0;
const float ST2084_c1 = 3424.0 / 4096.0;
const float ST2084_c2 = 2413.0 / 128.0;
const float ST2084_c3 = 2392.0 / 128.0;

vec3 LinearToPQ(vec3 color, float peakNits)
{
    // Map scene linear HDR (0..peakNits) to [0..1] PQ
    vec3 L = clamp(color / peakNits, 0.0, 1.0) * 10000.0; // normalize to 10000 nits max
    vec3 num = vec3(ST2084_c1) + vec3(ST2084_c2) * pow(L, vec3(ST2084_m1));
    vec3 denom = vec3(1.0) + vec3(ST2084_c3) * pow(L, vec3(ST2084_m1));
    return pow(num / denom, vec3(ST2084_m2));
}

#endif