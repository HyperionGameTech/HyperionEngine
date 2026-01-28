#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

#define DDGI

#define HYP_NO_CUBEMAP

HYP_DESCRIPTOR_SAMPLER(DDGI, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(DDGI, SamplerLinear) uniform sampler sampler_linear;

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

HYP_DESCRIPTOR_BUFFER(DDGI, DDGIConstants) uniform CBuffer
{
    DDGIConstants ddgiConstants;
};

/* Shadows */

HYP_DESCRIPTOR_SRV(DDGI, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(DDGI, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/shadows.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#undef HYP_NO_CUBEMAP

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

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

layout(buffer_reference, scalar) readonly buffer PackedVertexBuffer { float vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uvec3 indices[]; };

HYP_DESCRIPTOR_SRV(DDGI, EntitiesBuffer) readonly buffer EntitiesBuffer
{
    Entity entities[];
};

HYP_DESCRIPTOR_UAV(DDGI, MeshDescriptionsBuffer) buffer MeshDescriptionsBuffer
{
    MeshDescription mesh_descriptions[];
};

HYP_DESCRIPTOR_SRV(DDGI, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material materials[];
};

HYP_DESCRIPTOR_SRV(GlobalTextureSet, Textures) uniform texture2D textures[];

void main()
{

    MeshDescription mesh_description = mesh_descriptions[gl_InstanceCustomIndexEXT];
    PackedVertexBuffer vertex_buffer = PackedVertexBuffer(mesh_description.vertex_buffer_address);
    IndexBuffer index_buffer = IndexBuffer(mesh_description.index_buffer_address);

    uvec3 index = uvec3(
        index_buffer.indices[gl_PrimitiveID]
    );
    // payload.color = vec3(greaterThan(index, ivec3(mesh_description.num_vertices)));
    // return;
    
    Vertex v0;
    {
        const uint offset = 8 * index[0];
        v0.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v0.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v0.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }

    Vertex v1;
    {
        const uint offset = 8 * index[1];
        v1.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v1.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v1.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }
    Vertex v2;
    {
        const uint offset = 8 * index[2];
        v2.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v2.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v2.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }
    // payload.color = vec3(v0.position);
    // return;

    v0.position = (gl_ObjectToWorldEXT * vec4(v0.position, 1.0)).xyz;
    v1.position = (gl_ObjectToWorldEXT * vec4(v1.position, 1.0)).xyz;
    v2.position = (gl_ObjectToWorldEXT * vec4(v2.position, 1.0)).xyz;

    // Interpolate normal
    const vec3 barycentric_coords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 normal = normalize((gl_ObjectToWorldEXT * vec4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const vec2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    const vec3 position = v0.position * barycentric_coords.x + v1.position * barycentric_coords.y + v2.position * barycentric_coords.z;
    
    vec4 material_color = vec4(1.0);

    const uint32_t material_index = mesh_description.material_index;

    
    Material material;

    if (material_index != ~0u) {
        material = materials[material_index];
    }
    
    material_color = material.albedo;

    if (HAS_TEXTURE(material, AlbedoMap)) {
        vec4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(material, AlbedoMap, vec2(texcoord.x, 1.0 - texcoord.y));
        
        material_color *= albedo_texture;
    }

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (HAS_TEXTURE(material, MetalnessMap)) {
        float metalness_sample = SAMPLE_MATERIAL_TEXTURE(material, MetalnessMap, vec2(texcoord.x, 1.0 - texcoord.y)).r;
        
        metalness = metalness_sample;
    }

    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);

    if (HAS_TEXTURE(material, RoughnessMap)) {
        float roughness_sample = SAMPLE_MATERIAL_TEXTURE(material, RoughnessMap, vec2(texcoord.x, 1.0 - texcoord.y)).r;
        
        roughness = roughness_sample;
    }

    payload.throughput = vec4(material_color.rgb * (1.0 - metalness), 1.0);
    payload.emissive = vec4(0.0);
    payload.distance = gl_HitTEXT;
    payload.normal = normal;
    payload.roughness = roughness;
}
