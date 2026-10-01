#include "../Include/Defines.hlsli"

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
    float4 output_color : SV_Target0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV(DeferredPass, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(DeferredPass, GBufferDepthTexture) Texture2D GBufferDepthTexture;

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
    EnvProbe skyProbe; // textureIndices is ~0 without one
    GlimmerApply glimmer;
};

#define GLIMMER_APPLY_WITH_SAMPLING
#include "GlimmerApply.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const float2 texcoord = input.texcoord;

    uint2 gbufferDimensions;
    GBufferNormalsTexture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const int3 pixelCoord = int3(uint2(texcoord * gbufferDimensions), 0);

    const float3 N = normalize(GBufferUnpackNormal(GBufferNormalsTexture.Load(pixelCoord)));
    const float depth = GBufferDepthTexture.Load(pixelCoord).r;

    // texcoord y runs opposite to NDC y
    const float2 unjitteredTexcoord = texcoord - float2(camera.jitter.x, -camera.jitter.y) * 0.5;

    float4 positionWS = mul(camera.invViewMat, ReconstructViewSpacePositionFromDepth(camera.invProjMat, unjitteredTexcoord, depth));
    positionWS /= positionWS.w;

    if (glimmer.params.z != 0u)
    {
        output.output_color = float4(EvaluateGlimmerSHDebug(glimmer, positionWS.xyz, N), 1.0);
    }
    else if (glimmer.params.x == GLIMMER_DEBUG_VIS_COVERAGE)
    {
        output.output_color = float4(EvaluateGlimmerCoverage(glimmer, positionWS.xyz, N), 1.0);
    }
    else
    {
        // GLIMMER_DEBUG_VIS_IRRADIANCE shows this as it is
        output.output_color = EvaluateGlimmer(glimmer, positionWS.xyz, N);
    }

    return output;
}

#endif // PIXEL_SHADER
