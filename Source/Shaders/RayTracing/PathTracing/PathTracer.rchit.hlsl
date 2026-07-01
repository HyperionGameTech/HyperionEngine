#define PATHTRACER

#include "../../include/Defines.hlsli"
#include "../../include/Shared.hlsli"

#define HYP_NO_CUBEMAP

DECLARE_SAMPLER(PathTracer, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(PathTracer, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#include "../../include/Vertex.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/Material.hlsli"
#include "../../include/Entity.hlsli"
#include "../../include/Scene.hlsli"
#include "../../include/Noise.hlsli"

#include "../../include/BRDF.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/RayTracing/Mesh.hlsli"
#include "../../include/RayTracing/Payload.hlsli"

/* Shadows */

DECLARE_SRV(PathTracer, ShadowMapsTextureArray) Texture2DArray<float> shadow_maps;
DECLARE_SRV(PathTracer, PointLightShadowMapsTextureArray) TextureCubeArray point_shadow_maps;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/Shadows.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/RayTracing/RayTracingHelpers.hlsli"

/* End Shadows */

#undef HYP_NO_CUBEMAP

DECLARE_SRV(PathTracer, MaterialsBuffer) StructuredBuffer<Material> materials;

DECLARE_SRV(PathTracer, MeshDescriptionsBuffer) StructuredBuffer<MeshDescription> mesh_descriptions;

DECLARE_BUFFER_DYNAMIC(PathTracer, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Camera camera;
    Light lights[MAX_LIGHTS];
    ShadowMap shadowMaps[MAX_LIGHTS];
};

DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
DECLARE_SRV(BindlessResources1, Buffers) ByteAddressBuffer buffers[];

[shader("closesthit")]
void ClosestHitMain(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attrib)
{
    MeshDescription mesh_description = mesh_descriptions[InstanceIndex()];

    if (mesh_description.bindlessIndex == ~0u)
    {
        /// BAD mesh!
        payload.throughput = 0.0f;
        return;
    }
    
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

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    if (HAS_TEXTURE(material, DiffuseMap))
    {
        float4 albedo_texture = SAMPLE_MATERIAL_TEXTURE(material, DiffuseMap, float2(texcoord.x, 1.0 - texcoord.y));

        material_color *= albedo_texture;
    }

    if (HAS_TEXTURE(material, MetalnessMap))
    {
        float metalness_sample = SAMPLE_MATERIAL_TEXTURE(material, MetalnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;

        metalness = metalness_sample;
    }

    if (HAS_TEXTURE(material, RoughnessMap))
    {
        float roughness_sample = SAMPLE_MATERIAL_TEXTURE(material, RoughnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;

        roughness = roughness_sample;
    }
#endif // HYP_FEATURES_BINDLESS_TEXTURES

    // roughness is authorized as "perceptual roughness", we need to convert it to "physical roughness" for the BRDF calculations
    roughness = roughness * roughness;

    payload.throughput = float4(material_color.rgb, metalness);
    payload.emissive = float4(GET_MATERIAL_PARAM_FLOAT3(material, MATERIAL_PARAM_EMISSIVE_COLOR), 1.0) * GET_MATERIAL_PARAM(material, MATERIAL_PARAM_EMISSIVE_INTENSITY);
    payload.distance = RayTCurrent();
    payload.normal = normal;
    payload.roughness = roughness;
}
