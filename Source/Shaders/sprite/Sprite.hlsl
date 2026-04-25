#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Entity.hlsli"
#include "../include/GBuffer.hlsli"
#include "./Sprite.hlsli"

PERMUTE(SPRITE);

STATIC(INSTANCING);

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

#include "../include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
    Entity entity;
};

DECLARE_SRV_DYNAMIC(Default, SpriteInstanceBuffer) StructuredBuffer<SpriteInstanceData> SpriteInstanceBuffer;

struct SpriteInstanceData
{
    float4 positionSize;
    float4 color;
};

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    SpriteInstanceData instance = SpriteInstanceBuffer[instanceId];

    float3 center = instance.positionSize.xyz;
    float size = instance.positionSize.w;

    float3 localPos = float3((input.a_position.x - 0.5f) * size, (input.a_position.y - 0.5f) * size, 0.0f);

    float3 worldPos = center + localPos;

    float4 ndcPos = mul(camera.viewProjMat, float4(worldPos, 1.0));

    output.position_cs = ndcPos;
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

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
    Material material;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    output.gbuffer_albedo = float4(1.0, 0.0, 0.0, 1.0);//input.color * material.albedo;
    output.gbuffer_normals = GBufferPackNormal(float3(0.5f, 0.5f, 1.0f));
    output.gbuffer_material = uint4(0, 0, 0, 0);
    output.gbuffer_velocity = float2(0.0f, 0.0f);

    return output;
}

#endif // PIXEL_SHADER