#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Entity.hlsli"
#include "../include/GBuffer.hlsli"
#include "../include/Scene.hlsli"
#include "./Sprite.hlsli"

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
};

DECLARE_BUFFER_DYNAMIC(Sprite, CBuffer) cbuffer CBuffer
{
    Camera camera;
};

struct SpriteInstanceData
{
    float4 positionSize;
    float4 color;
    uint4 flags; // x = alwaysFaceCamera
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

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint4 gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    output.gbuffer_albedo = input.color;
    output.gbuffer_normals = GBufferPackNormal(float3(0.5f, 0.5f, 1.0f));
    output.gbuffer_material = uint4(0, 0, 0, 0);
    output.gbuffer_velocity = float2(0.0f, 0.0f);

    return output;
}

#endif // PIXEL_SHADER