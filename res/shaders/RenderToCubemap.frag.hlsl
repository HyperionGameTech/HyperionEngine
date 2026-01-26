#include "include/defines.inc"

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    nointerpolation float3 camera_position : TEXCOORD3;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint cube_face_index : TEXCOORD7;
};

struct PSOutput
{
#ifdef MODE_SHADOWS
    float2 output_color : SV_Target0;
#else
    float4 output_color : SV_Target0;
#endif

#ifdef WRITE_NORMALS
    float2 output_normals : SV_Target1;
    #ifdef WRITE_MOMENTS
    float2 output_moments : SV_Target2;
    #endif
#else
    #ifdef WRITE_MOMENTS
    float2 output_moments : SV_Target1;
    #endif
#endif
};

HYP_DESCRIPTOR_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#include "include/scene.inc"
#include "include/shared.inc"
#include "include/material.inc"
#include "include/gbuffer.inc"
#include "include/env_probe.inc"
#include "include/Octahedron.glsl"
#include "include/Entity.inc"
#include "include/packing.inc"
#include "include/brdf.inc"

#define HYP_CUBEMAP_AMBIENT 0.005

HYP_DESCRIPTOR_SRV(Default, ShadowMapsTextureArray) Texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(Default, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#ifdef INSTANCING
HYP_DESCRIPTOR_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
#else
HYP_DESCRIPTOR_SRV_DYNAMIC(Default, CurrentEntity) StructuredBuffer<Entity> entities;
#endif

HYP_DESCRIPTOR_SRV_DYNAMIC(Default, CurrentLight) StructuredBuffer<Light> current_light_buffer;
#define light current_light_buffer[0]

HYP_DESCRIPTOR_SRV_DYNAMIC(Default, MaterialsBuffer) StructuredBuffer<Material> materials_buffer;
#define material materials_buffer[0]

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3 V = normalize(input.camera_position - input.position);
    float3 N = normalize(input.normal);
    float3 R = reflect(-V, N);

    float4 albedo = float4(1.0, 1.0, 1.0, 1.0);

#if HAS_ALBEDO_MAP
    float2 texcoord = input.texcoord0 * CURRENT_MATERIAL.uv_scale;
    albedo = CURRENT_MATERIAL.albedo;

    float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(CURRENT_MATERIAL, AlbedoMap, texcoord);

    if (albedo_texture.a < 0.2)
    {
        discard;
    }

    albedo *= albedo_texture;
#endif

#if defined(WRITE_MOMENTS) || defined(MODE_SHADOWS)
    const float dist = distance(input.position, input.camera_position);
    float2 moments = float2(dist, HYP_FMATH_SQR(dist));
#endif

#ifdef MODE_SHADOWS
    float dx = ddx(dist);
    float dy = ddy(dist);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output.output_color = moments;
#else
    output.output_color.rgb = albedo.rgb;
    output.output_color.a = 1.0;

    #ifdef WRITE_NORMALS
    output.output_normals = PackNormalVec2(N);
    #endif

    #ifdef WRITE_MOMENTS
    output.output_moments = moments;
    #endif
#endif

    return output;
}
