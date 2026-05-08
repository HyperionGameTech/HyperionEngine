#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Entity.hlsli"
#include "../include/GBuffer.hlsli"
#include "../include/Scene.hlsli"
#include "./Sprite.hlsli"

struct TextSpriteInstanceData
{
    // 0
    float4x4 transform;
    // 64
    float2 texcoordStart;
    // 72
    float2 texcoordEnd;
    // 80
    uint textureIndex;
    // 84
    uint colorPacked;
    uint _pad1;
    uint _pad2;

    // 96
};

DECLARE_SRV(TextSprite, TextSpriteInstanceBuffer) StructuredBuffer<TextSpriteInstanceData> TextSpriteInstanceBuffer;

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
    uint instanceId : TEXCOORD2;
};

DECLARE_BUFFER_DYNAMIC(TextSprite, CBuffer) cbuffer CBuffer
{
    Camera camera;
};

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    TextSpriteInstanceData instance = TextSpriteInstanceBuffer[instanceId];

    float3 localPos = input.a_position;

    float4 worldPos = mul(instance.transform, float4(localPos, 1.0f));
    output.position_cs = mul(camera.viewProjMat, worldPos);

    output.texcoord0 = float2(
        lerp(instance.texcoordStart.x, instance.texcoordEnd.x, input.a_texcoord0.x),
        lerp(instance.texcoordStart.y, instance.texcoordEnd.y, input.a_texcoord0.y)
    );
    output.color = UINT_TO_VEC4(instance.colorPacked);
    output.instanceId = instanceId;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

DECLARE_SAMPLER(TextSprite, SamplerLinear) SamplerState sampler_linear;

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 texcoord0 : TEXCOORD0;
    float4 color : TEXCOORD1;
    uint instanceId : TEXCOORD2;
    bool isFrontFace : SV_IsFrontFace;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint4 gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D<float4> fontTextures[];
#else
//DECLARE_SRV(TextSprite, FontTexture) Texture2D<float4> FontTexture;
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    TextSpriteInstanceData instance = TextSpriteInstanceBuffer[input.instanceId];

    float2 uv = input.texcoord0;
    if (!input.isFrontFace)
    {
        uv.x = instance.texcoordStart.x + instance.texcoordEnd.x - uv.x;
    }

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    float4 texColor = (float4)0;

    if (instance.textureIndex != ~0u)
    {
        texColor = fontTextures[instance.textureIndex].Sample(sampler_linear, uv);
    }
    else
    {
        texColor = float4(1.0f, 0.0f, 1.0f, 1.0f); // jsut so we can see when something is wrong with the texture index
    }
#else
    float4 texColor = (float4)0.0;//FontTexture.Sample(sampler_linear, input.texcoord0);
#endif

    float4 color = input.color;
    color *= texColor.r;

    if (color.a < 0.01f)
    {
        discard;
    }

    output.gbuffer_albedo = color;
    output.gbuffer_normals = GBufferPackNormal(float3(0.5f, 0.5f, 1.0f));
    output.gbuffer_material = uint4(0, 0, 0, 0);
    output.gbuffer_velocity = float2(0, 0);

    return output;
}

#endif // PIXEL_SHADER
