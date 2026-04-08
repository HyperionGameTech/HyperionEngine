#define DDGI

#include "../../include/defines.inc"
#include "../../include/shared.inc"

#define HYP_NO_CUBEMAP

DECLARE_SAMPLER(DDGI, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(DDGI, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/material.inc"
#include "../../include/Entity.inc"
#include "../../include/scene.inc"

#include "../../include/BRDF.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"
#include "../../include/rt/payload.inc"

#include "../../include/rt/probe/probe_uniforms.inc"

DECLARE_BUFFER_DYNAMIC(DDGI, CBuffer) cbuffer CBuffer
{
    DDGIConstants ddgiConstants;
};

DECLARE_SRV(DDGI, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(DDGI, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#include "../../include/Shadows.hlsli"

#undef HYP_NO_CUBEMAP

DECLARE_SRV(DDGI, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV(DDGI, MaterialsBuffer) StructuredBuffer<Material> materials;

DECLARE_SRV(DDGI, MeshDescriptionsBuffer) StructuredBuffer<MeshDescription> mesh_descriptions;

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

    const float3 barycentric_coords = float3(1.0 - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
    const float3 normal = normalize(mul(ObjectToWorld3x4(), float4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const float2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    const float3 position = mul(ObjectToWorld3x4(), float4(v0.position * barycentric_coords.x + v1.position * barycentric_coords.y + v2.position * barycentric_coords.z, 1.0)).xyz;
    
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
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE_LOD(material, DiffuseMap, float2(texcoord.x, 1.0 - texcoord.y), 0.0);
        
        material_color *= albedo_texture;
    }

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (HAS_TEXTURE(material, MetalnessMap))
    {
        float metalness_sample = SAMPLE_MATERIAL_TEXTURE_LOD(material, MetalnessMap, float2(texcoord.x, 1.0 - texcoord.y), 0.0).r;
        
        metalness = metalness_sample;
    }

    payload.throughput = float4(material_color.rgb, metalness);
    payload.emissive = float4(GET_MATERIAL_PARAM_FLOAT3(material, MATERIAL_PARAM_EMISSIVE_FACTOR), 1.0);
    payload.distance = RayTCurrent();
    payload.normal = normal;
    payload.roughness = 1.0;
}
