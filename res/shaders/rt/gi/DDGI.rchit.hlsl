#define DDGI

#define HYP_NO_CUBEMAP

HYP_DESCRIPTOR_SAMPLER(DDGI, SamplerNearest) SamplerState sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(DDGI, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../../include/defines.inc"
#include "../../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/material.inc"
#include "../../include/Entity.inc"
#include "../../include/scene.inc"

#include "../../include/brdf.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"
#include "../../include/rt/payload.inc"

#include "../../include/rt/probe/probe_uniforms.inc"

HYP_DESCRIPTOR_BUFFER(DDGI, DDGIConstants) cbuffer CBuffer
{
    DDGIConstants ddgiConstants;
};

HYP_DESCRIPTOR_SRV(DDGI, ShadowMapsTextureArray) Texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(DDGI, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/shadows.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#undef HYP_NO_CUBEMAP

struct PayloadData
{
    float4 throughput;
    float4 color;
    float4 emissive;
    float distance;
    float3 normal;
    float roughness;
};

struct Attributes
{
    float2 bary;
};

struct PackedVertex
{
    float position_x;
    float position_y;
    float position_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float texcoord_s;
    float texcoord_t;
};

HYP_DESCRIPTOR_SRV(DDGI, EntitiesBuffer) StructuredBuffer<Entity> entities;
HYP_DESCRIPTOR_SRV(DDGI, MaterialsBuffer) StructuredBuffer<Material> materials;
HYP_DESCRIPTOR_SRV(GlobalTextureSet, Textures) Texture2D textures[];

HYP_DESCRIPTOR_SRV(DDGI, MeshDescriptionsBuffer) StructuredBuffer<MeshDescription> mesh_descriptions;

[shader("closesthit")]
void ClosestHitMain(inout PayloadData payload, in Attributes attrib)
{
    MeshDescription mesh_description = mesh_descriptions[InstanceIndex()];

    uint3 index = uint3(
        Load3x16BitIndices(mesh_description.index_buffer_address, PrimitiveIndex())
    );
    
    Vertex v0, v1, v2;
    
    {
        const uint offset = 8 * index[0];
        v0.position = float3(
            LoadFloat(mesh_description.vertex_buffer_address + offset * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 1) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 2) * 4)
        );
        
        v0.normal = float3(
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 3) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 4) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 5) * 4)
        );
        
        v0.texcoord0 = float2(
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 6) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 7) * 4)
        );
    }

    {
        const uint offset = 8 * index[1];
        v1.position = float3(
            LoadFloat(mesh_description.vertex_buffer_address + offset * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 1) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 2) * 4)
        );
        
        v1.normal = float3(
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 3) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 4) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 5) * 4)
        );
        
        v1.texcoord0 = float2(
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 6) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 7) * 4)
        );
    }
    
    {
        const uint offset = 8 * index[2];
        v2.position = float3(
            LoadFloat(mesh_description.vertex_buffer_address + offset * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 1) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 2) * 4)
        );
        
        v2.normal = float3(
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 3) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 4) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 5) * 4)
        );
        
        v2.texcoord0 = float2(
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 6) * 4),
            LoadFloat(mesh_description.vertex_buffer_address + (offset + 7) * 4)
        );
    }

    v0.position = mul(ObjectToWorld3x4(), float4(v0.position, 1.0)).xyz;
    v1.position = mul(ObjectToWorld3x4(), float4(v1.position, 1.0)).xyz;
    v2.position = mul(ObjectToWorld3x4(), float4(v2.position, 1.0)).xyz;

    const float3 barycentric_coords = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    const float3 normal = normalize(mul(ObjectToWorld3x4(), float4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const float2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    const float3 position = v0.position * barycentric_coords.x + v1.position * barycentric_coords.y + v2.position * barycentric_coords.z;
    
    float4 material_color = float4(1.0, 1.0, 1.0, 1.0);

    const uint material_index = mesh_description.material_index;

    Material material;

    if (material_index != ~0u) {
        material = materials[material_index];
    }
    
    material_color = material.albedo;

    if (HAS_TEXTURE(material, AlbedoMap)) {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(material, AlbedoMap, float2(texcoord.x, 1.0 - texcoord.y));
        
        material_color *= albedo_texture;
    }

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (HAS_TEXTURE(material, MetalnessMap)) {
        float metalness_sample = SAMPLE_MATERIAL_TEXTURE(material, MetalnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;
        
        metalness = metalness_sample;
    }

    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);

    if (HAS_TEXTURE(material, RoughnessMap)) {
        float roughness_sample = SAMPLE_MATERIAL_TEXTURE(material, RoughnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;
        
        roughness = roughness_sample;
    }

    payload.throughput = float4(material_color.rgb * (1.0 - metalness), 1.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = RayTCurrent();
    payload.normal = normal;
    payload.roughness = roughness;
}
