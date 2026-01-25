#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord0;
layout(location = 0) out vec4 color_output;

#include "include/scene.inc"
#include "include/PostFXInstance.inc"

#define HYP_FFXA_IMPL 2

#if (HYP_FFXA_IMPL == 2)
void main()
{
    float FXAA_SPAN_MAX = 8.0;
    float FXAA_REDUCE_MUL = 1.0 / 8.0;
    float FXAA_REDUCE_MIN = 1.0 / 128.0;

    vec2 resolution = vec2(camera.dimensions.xy);

    vec3 rgbNW = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (vec2(-1.0, -1.0) / resolution)).xyz;
    vec3 rgbNE = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (vec2(1.0, -1.0) / resolution)).xyz;
    vec3 rgbSW = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (vec2(-1.0, 1.0) / resolution)).xyz;
    vec3 rgbSE = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (vec2(1.0, 1.0) / resolution)).xyz;
    vec3 rgbM = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0).xyz;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
                  dir * rcpDirMin))
        / resolution;

    vec3 rgbA = (1.0 / 2.0) * (SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (1.0 / 3.0 - 0.5)).xyz + SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (0.0 / 3.0 - 0.5)).xyz + SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (3.0 / 3.0 - 0.5)).xyz);
    float lumaB = dot(rgbB, luma);

    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        color_output = vec4(rgbA, 1.0);
    }
    else
    {
        color_output = vec4(rgbB, 1.0);
    }
}
#endif

#if (HYP_FFXA_IMPL == 1)

#include "include/PostFXInstance.inc"

#define TextureSize vec2(camera.dimensions.xy)
#define OutputSize vec2(camera.dimensions.xy)

// compatibility #defines
#define Source gbuffer_deferred_result
#define vTexCoord v_texcoord0.xy
#define SourceSize vec4(TextureSize, 1.0 / TextureSize)
#define outsize vec4(OutputSize, 1.0 / OutputSize)
#define FXAA_SAMPLE(pos) SamplePrevEffectInChain(pos, vec4(0.0))

/*
FXAA_PRESET - Choose compile-in knob preset 0-5.
------------------------------------------------------------------------------
FXAA_EDGE_THRESHOLD - The minimum amount of local contrast required
                      to apply algorithm.
                      1.0/3.0  - too little
                      1.0/4.0  - good start
                      1.0/8.0  - applies to more edges
                      1.0/16.0 - overkill
------------------------------------------------------------------------------
FXAA_EDGE_THRESHOLD_MIN - Trims the algorithm from processing darks.
                          Perf optimization.
                          1.0/32.0 - visible limit (smaller isn't visible)
                          1.0/16.0 - good compromise
                          1.0/12.0 - upper limit (seeing artifacts)
------------------------------------------------------------------------------
FXAA_SEARCH_STEPS - Maximum number of search steps for end of span.
------------------------------------------------------------------------------
FXAA_SEARCH_THRESHOLD - Controls when to stop searching.
                        1.0/4.0 - seems to be the best quality wise
------------------------------------------------------------------------------
FXAA_SUBPIX_TRIM - Controls sub-pixel aliasing removal.
                   1.0/2.0 - low removal
                   1.0/3.0 - medium removal
                   1.0/4.0 - default removal
                   1.0/8.0 - high removal
                   0.0 - complete removal
------------------------------------------------------------------------------
FXAA_SUBPIX_CAP - Insures fine detail is not completely removed.
                  This is important for the transition of sub-pixel detail,
                  like fences and wires.
                  3.0/4.0 - default (medium amount of filtering)
                  7.0/8.0 - high amount of filtering
                  1.0 - no capping of sub-pixel aliasing removal
*/

#define FXAA_PRESET 5

#if (FXAA_PRESET == 3)
#define FXAA_EDGE_THRESHOLD (1.0 / 8.0)
#define FXAA_EDGE_THRESHOLD_MIN (1.0 / 16.0)
#define FXAA_SEARCH_STEPS 16
#define FXAA_SEARCH_THRESHOLD (1.0 / 4.0)
#define FXAA_SUBPIX_CAP (3.0 / 4.0)
#define FXAA_SUBPIX_TRIM (1.0 / 4.0)
#endif
#if (FXAA_PRESET == 4)
#define FXAA_EDGE_THRESHOLD (1.0 / 8.0)
#define FXAA_EDGE_THRESHOLD_MIN (1.0 / 24.0)
#define FXAA_SEARCH_STEPS 24
#define FXAA_SEARCH_THRESHOLD (1.0 / 4.0)
#define FXAA_SUBPIX_CAP (7.0 / 8.0)
#define FXAA_SUBPIX_TRIM (1.0 / 8.0)
#endif
#if (FXAA_PRESET == 5)
#define FXAA_EDGE_THRESHOLD (1.0 / 16.0)
#define FXAA_EDGE_THRESHOLD_MIN (1.0 / 16.0)
#define FXAA_SEARCH_STEPS 48
#define FXAA_SEARCH_THRESHOLD (1.0 / 4.0)
#define FXAA_SUBPIX_CAP (1.0)
#define FXAA_SUBPIX_TRIM (0.0)
#endif

#define FXAA_SUBPIX_TRIM_SCALE (1.0 / (1.0 - FXAA_SUBPIX_TRIM))

// Return the luma, the estimation of luminance from rgb inputs.
// This approximates luma using one FMA instruction,
// skipping normalization and tossing out blue.
// FxaaLuma() will range 0.0 to 2.963210702.
float FxaaLuma(vec3 rgb)
{
    return rgb.y * (0.587 / 0.299) + rgb.x;
}

vec3 FxaaLerp3(vec3 a, vec3 b, float amountOfA)
{
    return (vec3(-amountOfA) * b) + ((a * vec3(amountOfA)) + b);
}

vec4 FxaaTexOff(vec2 pos, ivec2 off, vec2 rcpFrame)
{
    float x = pos.x + float(off.x) * rcpFrame.x;
    float y = pos.y + float(off.y) * rcpFrame.y;
    return FXAA_SAMPLE(vec2(x, y));
}

// pos is the output of FxaaVertexShader interpolated across screen.
// xy -> actual texture position {0.0 to 1.0}
// rcpFrame should be a uniform equal to  {1.0/frameWidth, 1.0/frameHeight}
vec3 FxaaPixelShader(vec2 pos, vec2 rcpFrame)
{
    vec3 rgbN = FxaaTexOff(pos.xy, ivec2(0, -1), rcpFrame).xyz;
    vec3 rgbW = FxaaTexOff(pos.xy, ivec2(-1, 0), rcpFrame).xyz;
    vec3 rgbM = FxaaTexOff(pos.xy, ivec2(0, 0), rcpFrame).xyz;
    vec3 rgbE = FxaaTexOff(pos.xy, ivec2(1, 0), rcpFrame).xyz;
    vec3 rgbS = FxaaTexOff(pos.xy, ivec2(0, 1), rcpFrame).xyz;

    float lumaN = FxaaLuma(rgbN);
    float lumaW = FxaaLuma(rgbW);
    float lumaM = FxaaLuma(rgbM);
    float lumaE = FxaaLuma(rgbE);
    float lumaS = FxaaLuma(rgbS);
    float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));

    float range = rangeMax - rangeMin;
    if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
    {
        return rgbM;
    }

    vec3 rgbL = rgbN + rgbW + rgbM + rgbE + rgbS;

    float lumaL = (lumaN + lumaW + lumaE + lumaS) * 0.25;
    float rangeL = abs(lumaL - lumaM);
    float blendL = max(0.0, (rangeL / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_TRIM_SCALE;
    blendL = min(FXAA_SUBPIX_CAP, blendL);

    vec3 rgbNW = FxaaTexOff(pos.xy, ivec2(-1, -1), rcpFrame).xyz;
    vec3 rgbNE = FxaaTexOff(pos.xy, ivec2(1, -1), rcpFrame).xyz;
    vec3 rgbSW = FxaaTexOff(pos.xy, ivec2(-1, 1), rcpFrame).xyz;
    vec3 rgbSE = FxaaTexOff(pos.xy, ivec2(1, 1), rcpFrame).xyz;
    rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);
    rgbL *= vec3(1.0 / 9.0);

    float lumaNW = FxaaLuma(rgbNW);
    float lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW);
    float lumaSE = FxaaLuma(rgbSE);

    float edgeVert =
        abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) + abs((0.50 * lumaW) + (-1.0 * lumaM) + (0.50 * lumaE)) + abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));
    float edgeHorz =
        abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) + abs((0.50 * lumaN) + (-1.0 * lumaM) + (0.50 * lumaS)) + abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));

    bool horzSpan = edgeHorz >= edgeVert;
    float lengthSign = horzSpan ? -rcpFrame.y : -rcpFrame.x;

    if (!horzSpan)
    {
        lumaN = lumaW;
        lumaS = lumaE;
    }

    float gradientN = abs(lumaN - lumaM);
    float gradientS = abs(lumaS - lumaM);
    lumaN = (lumaN + lumaM) * 0.5;
    lumaS = (lumaS + lumaM) * 0.5;

    if (gradientN < gradientS)
    {
        lumaN = lumaS;
        lumaN = lumaS;
        gradientN = gradientS;
        lengthSign *= -1.0;
    }

    vec2 posN;
    posN.x = pos.x + (horzSpan ? 0.0 : lengthSign * 0.5);
    posN.y = pos.y + (horzSpan ? lengthSign * 0.5 : 0.0);

    gradientN *= FXAA_SEARCH_THRESHOLD;

    vec2 posP = posN;
    vec2 offNP = horzSpan ? vec2(rcpFrame.x, 0.0) : vec2(0.0, rcpFrame.y);
    float lumaEndN = lumaN;
    float lumaEndP = lumaN;
    bool doneN = false;
    bool doneP = false;
    posN += offNP * vec2(-1.0, -1.0);
    posP += offNP * vec2(1.0, 1.0);

    for (int i = 0; i < FXAA_SEARCH_STEPS; i++)
    {
        if (!doneN)
        {
            lumaEndN = FxaaLuma(FXAA_SAMPLE(posN.xy).xyz);
        }
        if (!doneP)
        {
            lumaEndP = FxaaLuma(FXAA_SAMPLE(posP.xy).xyz);
        }

        doneN = doneN || (abs(lumaEndN - lumaN) >= gradientN);
        doneP = doneP || (abs(lumaEndP - lumaN) >= gradientN);

        if (doneN && doneP)
        {
            break;
        }
        if (!doneN)
        {
            posN -= offNP;
        }
        if (!doneP)
        {
            posP += offNP;
        }
    }

    float dstN = horzSpan ? pos.x - posN.x : pos.y - posN.y;
    float dstP = horzSpan ? posP.x - pos.x : posP.y - pos.y;
    bool directionN = dstN < dstP;
    lumaEndN = directionN ? lumaEndN : lumaEndP;

    if (((lumaM - lumaN) < 0.0) == ((lumaEndN - lumaN) < 0.0))
    {
        lengthSign = 0.0;
    }

    float spanLength = (dstP + dstN);
    dstN = directionN ? dstN : dstP;
    float subPixelOffset = (0.5 + (dstN * (-1.0 / spanLength))) * lengthSign;
    vec3 rgbF = FXAA_SAMPLE(vec2(
                                pos.x + (horzSpan ? 0.0 : subPixelOffset),
                                pos.y + (horzSpan ? subPixelOffset : 0.0)))
                    .xyz;
    return FxaaLerp3(rgbL, rgbF, blendL);
}

void main()
{
    color_output = vec4(FxaaPixelShader(vTexCoord, vec2(SourceSize.z, SourceSize.w)), 1.0) * 1.0;
}

#endif