#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/Entity.inc"
#include "./UIObject.inc"

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

#define INSTANCING

#include "../include/scene.inc"

DECLARE_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) StructuredBuffer<UIEntityInstanceBatch> entity_instance_batch_buffer;
#define entity_instance_batch entity_instance_batch_buffer[0]

#undef OBJECT_INDEX
#define OBJECT_INDEX (entity_instance_batch.batch.indices[instanceId >> 2][instanceId & 3])

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float2 clamped_offset = entity_instance_batch.offsets[instanceId].xy;
    float2 size = entity_instance_batch.sizes[instanceId].xy;
    float2 clamped_size = entity_instance_batch.sizes[instanceId].zw;

    float4 position = mul(entity_instance_batch.batch.transforms[instanceId], float4(input.a_position, 1.0));
    float4 ndc_position = mul(camera.viewProjMat, position);

    float4 instance_texcoords = entity_instance_batch.texcoords[instanceId];

    float2 instance_texcoord_size = instance_texcoords.zw - instance_texcoords.xy;
    float2 clamped_instance_texcoord_size = instance_texcoord_size * (clamped_size / size);

    output.texcoord0 = instance_texcoords.xy - (clamped_offset / clamped_size * clamped_instance_texcoord_size) + (input.a_texcoord0 * clamped_instance_texcoord_size);

    output.object_index = OBJECT_INDEX;
    output.properties = entity_instance_batch.properties[instanceId];

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

#define INSTANCING

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/gbuffer.inc"
#include "../include/material.inc"
#include "../include/scene.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER_DYNAMIC(Default, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

DECLARE_SRV_DYNAMIC(Default, MaterialsBuffer) StructuredBuffer<Material> material_buffer;
#define material material_buffer[0]

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

#ifdef TEXTURED
#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#else
DECLARE_SRV(Default, DiffuseMap) Texture2D DiffuseMap;
#endif
#endif

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
#endif
    
#ifdef UI_TEXT
    // ui text uses R8 font atlas bitmap so swizzle red channel into rgba before mult by color value
    ui_color.rgba = (float4)albedo_texture.r;
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
