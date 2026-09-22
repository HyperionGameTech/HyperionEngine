#include "../include/Defines.hlsli"

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE float3 a_normal : NORMAL;
    HYP_ATTRIBUTE float2 a_texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = float4(input.a_position, 1.0);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

#include "../include/Gbuffer.hlsli"

DECLARE_SRV(DecalResolve, DecalNormalAccumTexture) Texture2D DecalNormalAccumTexture;
DECLARE_SRV(DecalResolve, DecalBaseNormalsTexture) Texture2D DecalBaseNormalsTexture;

struct PSInput
{
    float4 position : SV_POSITION;
};

struct PSOutput
{
    float4 normals : SV_Target0;
};

// only runs where the decal pass marked the stencil!
PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const int3 pixelCoord = int3(int2(input.position.xy), 0);

    const float4 normalAccum = DecalNormalAccumTexture.Load(pixelCoord);
    const float4 baseNormalsPacked = DecalBaseNormalsTexture.Load(pixelCoord);

    if (normalAccum.a <= 0.0)
    {
        output.normals = baseNormalsPacked;

        return output;
    }

    const float3 baseNormal = GBufferUnpackNormal(baseNormalsPacked);
    const float3 N = normalize(normalAccum.rgb + baseNormal * (1.0 - normalAccum.a));

    output.normals = GBufferPackNormal(N);
    // roughness + metalness share the target, keep the surface's
    output.normals.x = baseNormalsPacked.x;

    return output;
}

#endif // PIXEL_SHADER
