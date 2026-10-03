#include "../Include/Defines.hlsli"

// FULL evaluates every pixel (and the debug views), HALF one pixel of each 2x2 quad, UPSAMPLE fills the rest from HALF
PERMUTE(MODE, FULL, HALF, UPSAMPLE)

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
    float2 texcoord : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.texcoord = input.a_texcoord0;
    output.position_cs = float4(input.a_position, 1.0);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput
{
    float4 irradiance : SV_Target0; // rgb = irradiance / pi, a = weight; a < 0 where HALF skipped the pixel
    float2 specular : SV_Target1;   // GlimmerSHSpecularVisibility
    float3 reflection : SV_Target2; // irradiance / pi toward the reflection
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(DeferredPass, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;

DECLARE_SRV(DeferredPass, GlimmerHalfIrradianceTexture) Texture2D GlimmerHalfIrradianceTexture;
DECLARE_SRV(DeferredPass, GlimmerHalfSpecularTexture) Texture2D GlimmerHalfSpecularTexture;
DECLARE_SRV(DeferredPass, GlimmerHalfReflectionTexture) Texture2D GlimmerHalfReflectionTexture;

#include "../Include/Gbuffer.hlsli"
#include "../Include/Scene.hlsli"
#include "../Include/EnvProbes.hlsli"

DECLARE_SRV_DYNAMIC(DeferredPass, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(DeferredPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "GlimmerApply.hlsli"

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    GlimmerApply glimmer;
    uint4 irradianceParams; // xy = the pixel of each 2x2 quad HALF shades this frame
};

#define GLIMMER_APPLY_WITH_SAMPLING
#include "GlimmerApply.hlsli"

//////////UPSAMPLE////////////
#define GLIMMER_UPSAMPLE_PLANE_TOLERANCE 0.02
#define GLIMMER_UPSAMPLE_NORMAL_POWER 8.0
#define GLIMMER_UPSAMPLE_MIN_WEIGHT 0.05
//////////////////////////////

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

struct GlimmerSurface
{
    float3 P;
    float3 N;
    bool isValid;
};

GlimmerSurface GlimmerLoadSurface(int2 pixel, uint2 dimensions)
{
    GlimmerSurface surface;

    const float depth = GBufferDepthTexture.Load(int3(pixel, 0)).r;

    // texcoord y runs opposite to NDC y
    const float2 texcoord = (float2(pixel) + 0.5) / float2(dimensions);
    const float2 unjitteredTexcoord = texcoord - float2(camera.jitter.x, -camera.jitter.y) * 0.5;

    float4 positionWS = mul(camera.invViewMat, ReconstructViewSpacePositionFromDepth(camera.invProjMat, unjitteredTexcoord, depth));
    positionWS /= positionWS.w;

    surface.P = positionWS.xyz;
    surface.N = normalize(GBufferUnpackNormal(GBufferNormalsTexture.Load(int3(pixel, 0))));
    surface.isValid = depth < 1.0;

    return surface;
}

PSOutput GlimmerEvaluateSurface(GlimmerSurface surface)
{
    PSOutput output;

    const float3 V = normalize(camera.position.xyz - surface.P);
    const float3 R = reflect(-V, surface.N);

    output.irradiance = EvaluateGlimmer(glimmer, surface.P, surface.N, R, output.specular, output.reflection);

    return output;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    output.irradiance = (float4)0.0;
    output.specular = (float2)0.0;
    output.reflection = (float3)0.0;

    uint2 dimensions;
    GBufferNormalsTexture.GetDimensions(dimensions.x, dimensions.y);

#if defined(MODE_HALF)
    const uint2 halfDimensions = (dimensions + 1u) / 2u;
    const int2 pixel = min(int2(uint2(input.texcoord * halfDimensions)) * 2 + int2(irradianceParams.xy), int2(dimensions) - 1);

    const GlimmerSurface surface = GlimmerLoadSurface(pixel, dimensions);

    // no stencil at half res: sky and lightmapped pixels are skipped here
    if (!surface.isValid || ((GBufferMaterialTexture.Load(int3(pixel, 0)) >> 28u) & OBJECT_MASK_LIGHTMAPPED) != 0u)
    {
        output.irradiance = float4(0.0, 0.0, 0.0, -1.0);

        return output;
    }

    return GlimmerEvaluateSurface(surface);
#elif defined(MODE_UPSAMPLE)
    const int2 pixel = int2(uint2(input.texcoord * dimensions));
    const GlimmerSurface surface = GlimmerLoadSurface(pixel, dimensions);

    const int2 halfDimensions = int2((dimensions + 1u) / 2u);
    const int2 pick = int2(irradianceParams.xy);

    // half-res texel t holds pixel 2t + pick
    const float2 halfCoord = float2(pixel - pick) * 0.5;
    const int2 texel0 = int2(floor(halfCoord));
    const float2 fraction = halfCoord - float2(texel0);

    const float planeTolerance = GLIMMER_UPSAMPLE_PLANE_TOLERANCE * max(length(surface.P - camera.position.xyz), 1.0);

    float4 irradianceSum = (float4)0.0;
    float2 specularSum = (float2)0.0;
    float3 reflectionSum = (float3)0.0;
    float weightSum = 0.0;

    float4 bilinearIrradianceSum = (float4)0.0;
    float2 bilinearSpecularSum = (float2)0.0;
    float3 bilinearReflectionSum = (float3)0.0;
    float bilinearSum = 0.0;

    [unroll]
    for (uint corner = 0; corner < 4; corner++)
    {
        const int2 offset = int2(corner & 1u, corner >> 1);
        const int2 texel = texel0 + offset;

        if (any(texel < 0) || any(texel >= halfDimensions))
        {
            continue;
        }

        const float4 irradiance = GlimmerHalfIrradianceTexture.Load(int3(texel, 0));

        if (irradiance.a < 0.0)
        {
            continue;
        }

        const GlimmerSurface sampleSurface = GlimmerLoadSurface(min(texel * 2 + pick, int2(dimensions) - 1), dimensions);

        const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));
        const float planeDistance = abs(dot(sampleSurface.P - surface.P, surface.N)) / planeTolerance;

        const float bilinearWeight = bilinear.x * bilinear.y;
        const float weight = bilinearWeight
            * exp2(-planeDistance * planeDistance)
            * pow(saturate(dot(sampleSurface.N, surface.N)), GLIMMER_UPSAMPLE_NORMAL_POWER);

        const float2 specular = GlimmerHalfSpecularTexture.Load(int3(texel, 0)).rg;
        const float3 reflection = GlimmerHalfReflectionTexture.Load(int3(texel, 0)).rgb;

        irradianceSum += irradiance * weight;
        specularSum += specular * weight;
        reflectionSum += reflection * weight;
        weightSum += weight;

        bilinearIrradianceSum += irradiance * bilinearWeight;
        bilinearSpecularSum += specular * bilinearWeight;
        bilinearReflectionSum += reflection * bilinearWeight;
        bilinearSum += bilinearWeight;
    }

    // thin or isolated geometry none of the half-res pixels landed on
    if (weightSum < GLIMMER_UPSAMPLE_MIN_WEIGHT)
    {
        if (bilinearSum <= 0.0)
        {
            return output;
        }

        output.irradiance = bilinearIrradianceSum / bilinearSum;
        output.specular = bilinearSpecularSum / bilinearSum;
        output.reflection = bilinearReflectionSum / bilinearSum;

        return output;
    }

    output.irradiance = irradianceSum / weightSum;
    output.specular = specularSum / weightSum;
    output.reflection = reflectionSum / weightSum;

    return output;
#else
    const int2 pixel = int2(uint2(input.texcoord * dimensions));
    const GlimmerSurface surface = GlimmerLoadSurface(pixel, dimensions);

    if (glimmer.params.z != 0u)
    {
        output.irradiance = float4(EvaluateGlimmerSHDebug(glimmer, surface.P, surface.N), 1.0);
    }
    else if (glimmer.params.x == GLIMMER_DEBUG_VIS_COVERAGE)
    {
        output.irradiance = float4(EvaluateGlimmerCoverage(glimmer, surface.P, surface.N), 1.0);
    }
    else
    {
        // GLIMMER_DEBUG_VIS_IRRADIANCE shows this as it is
        output = GlimmerEvaluateSurface(surface);
    }

    return output;
#endif
}

#endif // PIXEL_SHADER
