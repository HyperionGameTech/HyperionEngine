#include "../include/defines.inc"
#include "../include/shared.inc"

#define HYP_NO_CUBEMAP

DECLARE_SAMPLER(RTReflections, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RTReflections, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/material.inc"
#include "../include/Entity.inc"
#include "../include/scene.inc"

#include "../include/brdf.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/rt/mesh.inc"
#include "../include/rt/payload.inc"

/* Shadows */

DECLARE_SRV(RTReflections, ShadowMapsTextureArray) Texture2DArray shadow_maps;
DECLARE_SRV(RTReflections, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shadows.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/rt/RTRadiance.inc"

/* End Shadows */

#undef HYP_NO_CUBEMAP

DECLARE_SRV(RTReflections, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV(RTReflections, MaterialsBuffer) StructuredBuffer<Material> materials;
DECLARE_SRV(RTReflections, MeshDescriptionsBuffer) StructuredBuffer<MeshDescription> mesh_descriptions;

DECLARE_BUFFER(RTReflections, RayTracingConstants) cbuffer RayTracingCBuffer
{
    RayTracingConstants rayTracingConstants;
};

DECLARE_BUFFER(RTReflections, Lights) cbuffer Lights
{
    Light lights[MAX_LIGHTS];
};

DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
DECLARE_SRV(BindlessResources1, Buffers) ByteAddressBuffer buffers[];

[shader("closesthit")]
void ClosestHitMain(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
    MeshDescription mesh_description = mesh_descriptions[InstanceIndex()];

    const uint vbIndex = mesh_description.bindlessIndex * 2;
    const uint ibIndex = mesh_description.bindlessIndex * 2 + 1;

    const uint3 index = uint3(
        buffers[ibIndex].Load(PrimitiveIndex() * 3 * 4 + 0),
        buffers[ibIndex].Load(PrimitiveIndex() * 3 * 4 + 4),
        buffers[ibIndex].Load(PrimitiveIndex() * 3 * 4 + 8)
    );

    Vertex v0, v1, v2;

    LoadVertex(buffers[vbIndex], index[0], v0);
    LoadVertex(buffers[vbIndex], index[1], v1);
    LoadVertex(buffers[vbIndex], index[2], v2);

    const float3 barycentric_coords = float3(
        1.0 - attrib.barycentrics.x - attrib.barycentrics.y,
        attrib.barycentrics.x,
        attrib.barycentrics.y
    );

    const float3 normal = normalize(mul(ObjectToWorld3x4(), float4(
        v0.normal * barycentric_coords.x +
        v1.normal * barycentric_coords.y +
        v2.normal * barycentric_coords.z,
        0.0
    )).xyz);

    const float2 texcoord =
        v0.texcoord0 * barycentric_coords.x +
        v1.texcoord0 * barycentric_coords.y +
        v2.texcoord0 * barycentric_coords.z;

    const float3 position = mul(ObjectToWorld3x4(), float4(
        v0.position * barycentric_coords.x +
        v1.position * barycentric_coords.y +
        v2.position * barycentric_coords.z,
        1.0
    )).xyz;

    float4 material_color = float4(1.0, 1.0, 1.0, 1.0);

    const uint material_index = mesh_description.material_index;

    Material material = (Material)0;

    if (material_index != ~0u)
    {
        material = materials[material_index];
    }

    if (HAS_TEXTURE(material, AlbedoMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(material, AlbedoMap, float2(texcoord.x, 1.0 - texcoord.y));

        material_color *= albedo_texture;
    }

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (HAS_TEXTURE(material, MetalnessMap))
    {
        float metalness_sample = SAMPLE_MATERIAL_TEXTURE(material, MetalnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;

        metalness = metalness_sample;
    }

    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);

    if (HAS_TEXTURE(material, RoughnessMap))
    {
        float roughness_sample = SAMPLE_MATERIAL_TEXTURE(material, RoughnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;

        roughness = roughness_sample;
    }

    const float ambient = 0.2;

    float4 indirect_lighting = material_color * (1.0 - metalness) * ambient;

    float4 direct_lighting = float4(0.0, 0.0, 0.0, 0.0);

    for (uint light_index = 0; light_index < rayTracingConstants.num_bound_lights; light_index++)
    {
        const Light light = lights[light_index];

        // Only support point and directional lights for RT reflections.
        if (light.type != HYP_LIGHT_TYPE_DIRECTIONAL && light.type != HYP_LIGHT_TYPE_POINT)
        {
            continue;
        }

        const float3 L = CalculateLightDirection(light, position);
        const float NdotL = max(dot(normal, L), 0.00001);

        const float2 radiusFalloff = unpackHalf2x16(light.radius_falloff);
        const float radius = radiusFalloff.x;

        const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
            ? GetSquareFalloffAttenuation(position.xyz, light.position_intensity.xyz, radius)
            : 1.0;

        float4 local_light = float4(NdotL, NdotL, NdotL, NdotL) * light.color * light.position_intensity.w * attenuation;

        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && bool(light.flags & LF_SHADOW))
        {
            local_light *= GetShadowStandard(light, position.xyz);
        }

        direct_lighting += material_color * local_light;
    }

    payload.color = indirect_lighting + direct_lighting;
    payload.distance = RayTCurrent();
    payload.normal = normal;
    payload.roughness = roughness;
}
