#include "include/Defines.hlsli"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.a_position, 1.0);

    output.position = position.xyz;
    output.texcoord = input.a_texcoord0;

    output.position_cs = position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color_output : SV_Target0;
};

#include "include/Scene.hlsli"
#include "include/PostFXInstance.hlsli"

#define HYP_FFXA_IMPL 2

#if (HYP_FFXA_IMPL == 2)

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float2 v_texcoord0 = input.texcoord;

    float FXAA_SPAN_MAX = 8.0;
    float FXAA_REDUCE_MUL = 1.0 / 8.0;
    float FXAA_REDUCE_MIN = 1.0 / 128.0;

    float2 resolution = float2(camera.dimensions.xy);

    float3 rgbNW = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (float2(-1.0, -1.0) / resolution)).xyz;
    float3 rgbNE = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (float2(1.0, -1.0) / resolution)).xyz;
    float3 rgbSW = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (float2(-1.0, 1.0) / resolution)).xyz;
    float3 rgbSE = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0 + (float2(1.0, 1.0) / resolution)).xyz;
    float3 rgbM = SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0).xyz;

    float3 luma = float3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
                  dir * rcpDirMin))
        / resolution;

    float3 rgbA = (1.0 / 2.0) * (SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (1.0 / 3.0 - 0.5)).xyz + SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (2.0 / 3.0 - 0.5)).xyz);
    float3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (0.0 / 3.0 - 0.5)).xyz + SAMPLE_TEXTURE_2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0.xy + dir * (3.0 / 3.0 - 0.5)).xyz);
    float lumaB = dot(rgbB, luma);

    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        output.color_output = float4(rgbA, 1.0);
    }
    else
    {
        output.color_output = float4(rgbB, 1.0);
    }

    return output;
}

#endif

#if (HYP_FFXA_IMPL == 1)

#define TextureSize float2(camera.dimensions.xy)
#define OutputSize float2(camera.dimensions.xy)

#define Source gbuffer_deferred_result
#define vTexCoord input.texcoord.xy
#define SourceSize float4(TextureSize, 1.0 / TextureSize)
#define outsize float4(OutputSize, 1.0 / OutputSize)
#define FXAA_SAMPLE(pos) SamplePrevEffectInChain(pos, float4(0.0, 0.0, 0.0, 0.0))

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

float FxaaLuma(float3 rgb)
{
    return rgb.y * (0.587 / 0.299) + rgb.x;
}

float3 FxaaLerp3(float3 a, float3 b, float amountOfA)
{
    return (float3(-amountOfA, -amountOfA, -amountOfA) * b) + ((a * float3(amountOfA, amountOfA, amountOfA)) + b);
}

float4 FxaaTexOff(float2 pos, int2 off, float2 rcpFrame)
{
    float x = pos.x + float(off.x) * rcpFrame.x;
    float y = pos.y + float(off.y) * rcpFrame.y;
    return FXAA_SAMPLE(float2(x, y));
}

float3 FxaaPixelShader(float2 pos, float2 rcpFrame)
{
    float3 rgbN = FxaaTexOff(pos.xy, int2(0, -1), rcpFrame).xyz;
    float3 rgbW = FxaaTexOff(pos.xy, int2(-1, 0), rcpFrame).xyz;
    float3 rgbM = FxaaTexOff(pos.xy, int2(0, 0), rcpFrame).xyz;
    float3 rgbE = FxaaTexOff(pos.xy, int2(1, 0), rcpFrame).xyz;
    float3 rgbS = FxaaTexOff(pos.xy, int2(0, 1), rcpFrame).xyz;

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

    float3 rgbL = rgbN + rgbW + rgbM + rgbE + rgbS;

    float lumaL = (lumaN + lumaW + lumaE + lumaS) * 0.25;
    float rangeL = abs(lumaL - lumaM);
    float blendL = max(0.0, (rangeL / range) - FXAA_SUBPIX_TRIM) * FXAA_SUBPIX_TRIM_SCALE;
    blendL = min(FXAA_SUBPIX_CAP, blendL);

    float3 rgbNW = FxaaTexOff(pos.xy, int2(-1, -1), rcpFrame).xyz;
    float3 rgbNE = FxaaTexOff(pos.xy, int2(1, -1), rcpFrame).xyz;
    float3 rgbSW = FxaaTexOff(pos.xy, int2(-1, 1), rcpFrame).xyz;
    float3 rgbSE = FxaaTexOff(pos.xy, int2(1, 1), rcpFrame).xyz;
    rgbL += (rgbNW + rgbNE + rgbSW + rgbSE);
    rgbL *= float3(1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0);

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

    float2 posN;
    posN.x = pos.x + (horzSpan ? 0.0 : lengthSign * 0.5);
    posN.y = pos.y + (horzSpan ? lengthSign * 0.5 : 0.0);

    gradientN *= FXAA_SEARCH_THRESHOLD;

    float2 posP = posN;
    float2 offNP = horzSpan ? float2(rcpFrame.x, 0.0) : float2(0.0, rcpFrame.y);
    float lumaEndN = lumaN;
    float lumaEndP = lumaN;
    bool doneN = false;
    bool doneP = false;
    posN += offNP * float2(-1.0, -1.0);
    posP += offNP * float2(1.0, 1.0);

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
    float lumaEndN = directionN ? lumaEndN : lumaEndP;

    if (((lumaM - lumaN) < 0.0) == ((lumaEndN - lumaN) < 0.0))
    {
        lengthSign = 0.0;
    }

    float spanLength = (dstP + dstN);
    dstN = directionN ? dstN : dstP;
    float subPixelOffset = (0.5 + (dstN * (-1.0 / spanLength))) * lengthSign;
    float3 rgbF = FXAA_SAMPLE(float2(
                                pos.x + (horzSpan ? 0.0 : subPixelOffset),
                                pos.y + (horzSpan ? subPixelOffset : 0.0)))
                    .xyz;
    return FxaaLerp3(rgbL, rgbF, blendL);
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    output.color_output = float4(FxaaPixelShader(vTexCoord, float2(SourceSize.z, SourceSize.w)), 1.0) * 1.0;

    return output;
}

#endif

#endif // PIXEL_SHADER
