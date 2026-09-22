#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Entity.hlsli"
#include "../include/GBuffer.hlsli"
#include "../include/Scene.hlsli"
#include "./Sprite.hlsli"

struct SpriteInstanceData
{
    float4 positionSize;
    float4 color;
    uint4 flags; // x = alwaysFaceCamera, y = bindless texture index (~0u = untextured)
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
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
    nointerpolation uint textureIndex : TEXCOORD2;
};

DECLARE_BUFFER_DYNAMIC(Sprite, CBuffer) cbuffer CBuffer
{
    Camera camera;
};

DECLARE_SRV(Sprite, SpriteInstanceBuffer) StructuredBuffer<SpriteInstanceData> SpriteInstanceBuffer;

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    SpriteInstanceData instance = SpriteInstanceBuffer[instanceId];

    float3 center = instance.positionSize.xyz;
    float size = instance.positionSize.w;

    float3 worldPos;

    if (instance.flags.x != 0u)
    {
        float3 camRight = float3(camera.view[0][0], camera.view[0][1], camera.view[0][2]);
        float3 camUp = float3(camera.view[1][0], camera.view[1][1], camera.view[1][2]);

        worldPos = center
            + camRight * (input.a_position.x - 0.5f) * size
            + camUp * (input.a_position.y - 0.5f) * size;
    }
    else
    {
        float3 localPos = float3((input.a_position.x - 0.5f) * size, (input.a_position.y - 0.5f) * size, 0.0f);
        worldPos = center + localPos;
    }

    output.position_cs = mul(camera.viewProjMat, float4(worldPos, 1.0));
    output.texcoord0 = input.a_texcoord0;
    output.color = instance.color;
    output.textureIndex = instance.flags.y;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

DECLARE_SAMPLER(Sprite, SamplerLinear) SamplerState sampler_linear;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D<float4> spriteTextures[];
#else
DECLARE_SRV(Sprite, SpriteTexture) Texture2D<float4> SpriteTexture;
#endif

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
    nointerpolation uint textureIndex : TEXCOORD2;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float4 color = input.color;

    const float2 uv = input.texcoord0;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    if (input.textureIndex != ~0u)
    {
        color *= spriteTextures[NonUniformResourceIndex(input.textureIndex)].Sample(sampler_linear, uv);
    }
#else
    if (input.textureIndex != ~0u)
    {
        color *= SpriteTexture.Sample(sampler_linear, uv);
    }
#endif

    if (color.a < 0.01f)
    {
        discard;
    }

    output.gbuffer_albedo = color;
    output.gbuffer_normals = GBufferPackNormal(float3(0.5f, 0.5f, 1.0f));
    output.gbuffer_material = 0;
    output.gbuffer_velocity = (float2)0;

    return output;
}

#endif // PIXEL_SHADER
