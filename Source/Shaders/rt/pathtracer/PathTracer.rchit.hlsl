#define PATHTRACER

#include "../../include/defines.inc"
#include "../../include/shared.inc"

#define HYP_NO_CUBEMAP

DECLARE_SAMPLER(PathTracer, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(PathTracer, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/material.inc"
#include "../../include/Entity.inc"
#include "../../include/scene.inc"
#include "../../include/noise.inc"

#include "../../include/brdf.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"
#include "../../include/rt/payload.inc"

/* Shadows */

DECLARE_SRV(PathTracer, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(PathTracer, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/Shadows.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/RayTracingHelpers.inc"

/* End Shadows */

#undef HYP_NO_CUBEMAP

DECLARE_SRV(PathTracer, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV(PathTracer, MaterialsBuffer) StructuredBuffer<Material> materials;

DECLARE_SRV(PathTracer, MeshDescriptionsBuffer) StructuredBuffer<MeshDescription> mesh_descriptions;

DECLARE_BUFFER_DYNAMIC(PathTracer, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

DECLARE_BUFFER_DYNAMIC(RTReflections, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
};

DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
DECLARE_SRV(BindlessResources1, Buffers) ByteAddressBuffer buffers[];

float CheckLightIntersection(in Light light, in float3 position, in float3 R)
{
    float3 L = CalculateLightDirection(light, position);

    if (light.type == 0)
    {
        return HYP_FMATH_INFINITY;
    }

    float3 light_to_position = position - light.position_intensity.xyz;
    float light_to_position_length = length(light_to_position);

    const float2 radiusFalloff = unpackHalf2x16(light.radiusFalloffPacked);
    const float radius = radiusFalloff.x;

    if (light_to_position_length > radius)
    {
        return HYP_FMATH_INFINITY;
    }

    return light_to_position_length;
}

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

    const float3 barycentric_coords = float3(1.0 - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
    const float3 normal = normalize(mul(ObjectToWorld3x4(), float4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const float2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    const float3 position = mul(ObjectToWorld3x4(), float4(v0.position * barycentric_coords.x + v1.position * barycentric_coords.y + v2.position * barycentric_coords.z, 1.0)).xyz;

    v0.position = mul(ObjectToWorld3x4(), float4(v0.position, 1.0)).xyz;
    v1.position = mul(ObjectToWorld3x4(), float4(v1.position, 1.0)).xyz;
    v2.position = mul(ObjectToWorld3x4(), float4(v2.position, 1.0)).xyz;

    const float3 hit_position = (WorldRayOrigin() + RayTCurrent() * WorldRayDirection()).xyz;

    float4 material_color = float4(1.0, 1.0, 1.0, 1.0);

    const uint material_index = mesh_description.material_index;

    Material material = (Material)0;

    if (material_index != ~0u)
    {
        material = materials[material_index];
    }
    
    material_color = material.albedo;

    if (HAS_TEXTURE(material, DiffuseMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(material, DiffuseMap, float2(texcoord.x, 1.0 - texcoord.y));
        
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

    // roughness is authorized as "perceptual roughness", we need to convert it to "physical roughness" for the BRDF calculations
    roughness = roughness * roughness;
    
    const float3 V = normalize(camera.position.xyz - position);
    const float NdotV = max(0.0001, dot(normal, V));

    const float material_reflectance = 0.5;
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    float4 F0 = float4(material_color.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);
    float4 F90 = float4(clamp(dot(F0, float4(50.0 * 0.33, 50.0 * 0.33, 50.0 * 0.33, 50.0 * 0.33)), 0.0, 1.0), 0.0, 0.0, 0.0);

    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.throughput = float4(material_color.rgb, metalness);
    payload.distance = RayTCurrent();
    payload.normal = normal;
    payload.roughness = roughness;
}
