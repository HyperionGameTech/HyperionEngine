#include "../include/Defines.hlsli"
#include "../include/Shared.hlsli"
#include "../include/Entity.hlsli"
#include "./UIObject.hlsli"

PERMUTE(TEXTURED);
PERMUTE(UI_TEXT);

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
    float3 position : POSITION;
    float3 screen_space_position : TEXCOORD1;
    float2 texcoord0 : TEXCOORD2;
    float4 color : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD4;
    nointerpolation uint4 properties : TEXCOORD5;
};

#include "../include/Scene.hlsli"

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer currentBatchBuffer;

#undef OBJECT_INDEX
#define OBJECT_INDEX (currentBatch.indices[instanceId >> 2][instanceId & 3])

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    UIEntityInstanceBatch currentBatch = currentBatchBuffer.Load<UIEntityInstanceBatch>(0);

    float4x4 transform = currentBatch.transforms[instanceId];
#ifdef VULKAN
    transform = transpose(transform);
#endif

    float2 clamped_offset = currentBatch.offsets[instanceId].xy;
    float2 size = currentBatch.sizes[instanceId].xy;
    float2 clamped_size = currentBatch.sizes[instanceId].zw;

    float4 position = mul(transform, float4(input.a_position, 1.0));

    float4 ndc_position = mul(camera.viewProjMat, position);
    ndc_position.y = -ndc_position.y;

    float2 texcoord = input.a_texcoord0;
    texcoord.y = 1.0 - texcoord.y;

    float4 instance_texcoords = currentBatch.texcoords[instanceId];

    float2 instance_texcoord_size = instance_texcoords.zw - instance_texcoords.xy;
    float2 clamped_instance_texcoord_size = instance_texcoord_size * (clamped_size / size);

    output.texcoord0 = instance_texcoords.xy - (clamped_offset / clamped_size * clamped_instance_texcoord_size) + (texcoord * clamped_instance_texcoord_size);

    output.object_index = OBJECT_INDEX;
    output.properties = currentBatch.properties[instanceId];

    output.position = position.xyz;
    output.screen_space_position = float3(ndc_position.xy * 0.5 + 0.5, ndc_position.z);

    output.color = float4(1.0, 1.0, 1.0, 1.0);

    output.position_cs = ndc_position;

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 screen_space_position : TEXCOORD1;
    float2 texcoord0 : TEXCOORD2;
    float4 color : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD4;
    nointerpolation uint4 properties : TEXCOORD5;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/Gbuffer.hlsli"
#include "../include/Material.hlsli"
#include "../include/Scene.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_SRV_DYNAMIC(Default, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif // CURRENT_MATERIAL

#ifdef TEXTURED
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#else // !HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Default, DiffuseMap) Texture2D DiffuseMap;
#endif // HYP_FEATURES_BINDLESS_TEXTURES
#endif // TEXTURED

float RoundedRectangle(float2 pos, float2 size, float radius)
{
    return 1.0 - clamp((length(max(abs(pos) - size + radius, 0.0)) - radius), 0.0, 1.0);
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    const UIObjectProperties properties = GetUIObjectProperties(input.properties);

    float4 ui_color = (float4)1.0;

#ifdef TEXTURED
    float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, DiffuseMap, input.texcoord0);

    ui_color *= albedo_texture;

#ifdef UI_TEXT
    // ui text uses R8 font atlas bitmap so swizzle red channel into rgba before mult by color value
    ui_color.rgba = (float4)albedo_texture.r;
#endif
#endif

    ui_color *= CURRENT_MATERIAL.albedo;

    float2 size = float2(properties.size);
    float2 position = input.texcoord0 * size;

    if (!bool(HYP_FAST_LESS(properties.border_flags, 1)) && !bool(HYP_FAST_LESS(properties.border_radius, HYP_FMATH_EPSILON)))
    {
        float roundedness = RoundedRectangle((size * 0.5) - position, size * 0.5, properties.border_radius);

        float top = float((properties.border_flags & UOB_TOP) != 0u);
        float left = float((properties.border_flags & UOB_LEFT) != 0u);
        float bottom = float((properties.border_flags & UOB_BOTTOM) != 0u);
        float right = float((properties.border_flags & UOB_RIGHT) != 0u);

        roundedness = lerp(lerp(roundedness, 1.0, step(0.5, 1.0 - input.texcoord0.y)), roundedness, top);
        roundedness = lerp(lerp(roundedness, 1.0, step(0.5, 1.0 - input.texcoord0.x)), roundedness, left);
        roundedness = lerp(lerp(roundedness, 1.0, step(0.5, input.texcoord0.y)), roundedness, bottom);
        roundedness = lerp(lerp(roundedness, 1.0, step(0.5, input.texcoord0.x)), roundedness, right);

        ui_color.a *= lerp(1.0, roundedness, 1.0 - step(properties.border_radius, 0.0));
    }

    output.gbuffer_albedo = ui_color * input.color;

    return output;
}

#endif // PIXEL_SHADER
