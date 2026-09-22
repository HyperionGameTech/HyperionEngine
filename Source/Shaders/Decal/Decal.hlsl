#include "../include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/Scene.hlsli"
#include "../include/Shared.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Decal.hlsli"

struct DecalInstanceData
{
    float4x4 worldToDecal;
};

DECLARE_SRV(Decal, DecalInstanceBuffer) StructuredBuffer<DecalInstanceData> DecalInstanceBuffer;

DECLARE_BUFFER_DYNAMIC(Decal, CBuffer) cbuffer CBuffer
{
    Camera camera;

    // struct DecalTypeShaderData {
    float4 tint;
    float opacity;
    float normalStrength;
    float angleFadeStart;
    float angleFadeEnd;
    uint excludeMask;
    uint flags;
    // SV_InstanceID doesn't include the base instance on DX12
    uint firstInstance;
    // }
};

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
    nointerpolation uint instanceIndex : TEXCOORD0;
};

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    const uint instanceIndex = firstInstance + instanceId;

    // @TODO Use decalToWorld instead...
    const float4x4 worldToDecal = DecalInstanceBuffer[instanceIndex].worldToDecal;

    const float3x3 rotationScale = (float3x3)worldToDecal;
    const float3 inverseScaleSquared = float3(dot(rotationScale[0], rotationScale[0]), dot(rotationScale[1], rotationScale[1]), dot(rotationScale[2], rotationScale[2]));
    const float3 worldOriginInDecal = float3(worldToDecal[0][3], worldToDecal[1][3], worldToDecal[2][3]);

    const float3 worldPosition = mul((input.a_position - worldOriginInDecal) / inverseScaleSquared, rotationScale);

    float4x4 jitterMat = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    jitterMat[0][3] += camera.jitter.x;
    jitterMat[1][3] += camera.jitter.y;

    output.position_cs = mul(jitterMat, mul(camera.viewProjMat, float4(worldPosition, 1.0)));
    output.instanceIndex = instanceIndex;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#define sampler_linear SamplerLinear
#define sampler_nearest SamplerNearest

DECLARE_SRV(Decal, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SRV(Decal, GBufferMaterialTexture) Texture2D<uint> GBufferMaterialTexture;
DECLARE_SRV(Decal, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_SRV(Decal, DecalAlbedoTexture) Texture2D DecalAlbedoTexture;
DECLARE_SRV(Decal, DecalNormalTexture) Texture2D DecalNormalTexture;

DECLARE_SAMPLER(Decal, SamplerNearest) SamplerState SamplerNearest;
DECLARE_SAMPLER(Decal, SamplerLinear) SamplerState SamplerLinear;

#include "../include/Gbuffer.hlsli"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    nointerpolation uint instanceIndex : TEXCOORD0;
};

struct PSOutput
{
    float4 albedo : SV_Target0;
    float4 normalAccum : SV_Target1;
    float4 baseNormals : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const DecalInstanceData instance = DecalInstanceBuffer[input.instanceIndex];

    uint2 dimensions;
    GBufferDepthTexture.GetDimensions(dimensions.x, dimensions.y);

    const int3 pixelCoord = int3(clamp(int2(input.position_cs.xy), (int2)0, int2(dimensions) - 1), 0);
    const float2 texcoord = (float2(pixelCoord.xy) + 0.5) / float2(dimensions);

    const float depth = GBufferDepthTexture.Load(pixelCoord).r;

    if (depth >= 1.0)
    {
        discard;
    }

    const float3 P = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, texcoord, depth).xyz;
    const float3 local = mul(instance.worldToDecal, float4(P, 1.0)).xyz;

    if (any(abs(local) > 1.0))
    {
        discard;
    }

    const uint objectMask = GBufferMaterialTexture.Load(pixelCoord) >> 28u;

    if ((objectMask & excludeMask) != 0)
    {
        discard;
    }

    const float4 baseNormalsPacked = GBufferNormalsTexture.Load(pixelCoord);
    const float3 baseNormal = GBufferUnpackNormal(baseNormalsPacked);

    // rows of worldToDecal are the decal axes
    // thsese guys are scaled ( 1 / axis scale )
    const float3 axisX = normalize(instance.worldToDecal[0].xyz);
    const float3 axisY = normalize(instance.worldToDecal[1].xyz);

    const float angleFade = saturate((dot(baseNormal, axisY) - angleFadeEnd) / max(angleFadeStart - angleFadeEnd, 0.0001));
    const float depthFade = saturate((1.0 - abs(local.y)) * 5.0);

    const float coverage = opacity * angleFade * depthFade;

    if (coverage <= 0.0)
    {
        discard;
    }

    const float2 uv = float2(local.x, -local.z) * 0.5 + 0.5;

    float4 albedo = tint;
    float mask = 1.0;

    if (flags & DECAL_FLAG_HAS_ALBEDO_MAP)
    {
        const float4 albedoSample = DecalAlbedoTexture.Sample(SamplerLinear, uv);

        albedo *= albedoSample;
        mask = albedoSample.a;
    }

    const float albedoAlpha = albedo.a * coverage;
    float normalAlpha = 0.0;
    float3 decalNormal = baseNormal;

    if (flags & DECAL_FLAG_HAS_NORMAL_MAP)
    {
        float3 tangentNormal = DecalNormalTexture.Sample(SamplerLinear, uv).xyz * 2.0 - 1.0;

        if (flags & DECAL_FLAG_NORMAL_MAP_FLIP_Y)
        {
            tangentNormal.y = -tangentNormal.y;
        }

        tangentNormal.xy *= normalStrength;

        const float3 T = normalize(axisX - baseNormal * dot(axisX, baseNormal));
        const float3 B = cross(T, baseNormal);

        decalNormal = normalize(T * tangentNormal.x + B * tangentNormal.y + baseNormal * tangentNormal.z);
        normalAlpha = mask * coverage;
    }

    if (albedoAlpha <= 0.001 && normalAlpha <= 0.001)
    {
        discard;
    }

    output.albedo = float4(albedo.rgb, albedoAlpha);
    output.normalAccum = float4(decalNormal * normalAlpha, normalAlpha);
    output.baseNormals = baseNormalsPacked;

    return output;
}

#endif // PIXEL_SHADER
