#define PATHTRACER
#define LIGHTMAPPER

#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Noise.hlsli"
#include "../../Include/Packing.hlsli"

PERMUTE(MODE, LIGHTMAP, IRRADIANCE, RADIANCE, MOMENTS, BENT_NORMALS);

DECLARE_SAMPLER(LightmapPathTracerCompute, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(LightmapPathTracerCompute, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#include "../../Include/BRDF.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SRV(LightmapPathTracerCompute, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SRV(LightmapPathTracerCompute, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

#include "../../Include/BlueNoise.hlsli"
#include "../../Include/EnvProbes.hlsli"

DECLARE_SRV(LightmapPathTracerCompute, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "../../Include/Octahedron.hlsli"

#include "../../Include/RayTracing/RayTracingHelpers.hlsli"
#include "../../Include/RayTracing/Payload.hlsli"
#include "../../Include/RayTracing/BVH.hlsli"

DECLARE_UAV(LightmapPathTracerCompute, HitsBuffer) RWStructuredBuffer<float4> hits;

DECLARE_SRV(LightmapPathTracerCompute, RaysBuffer) StructuredBuffer<float4> ray_data;

DECLARE_SRV(LightmapPathTracerCompute, MaterialsBuffer) StructuredBuffer<Material> materials;

DECLARE_SRV(LightmapPathTracerCompute, BVHNodesBuffer) StructuredBuffer<BVHNode> bvhNodes;
DECLARE_SRV(LightmapPathTracerCompute, BVHTrianglesBuffer) StructuredBuffer<BVHTriangle> bvhTriangles;
DECLARE_SRV(LightmapPathTracerCompute, BVHTriangleAttributesBuffer) StructuredBuffer<BVHTriangleAttributes> bvhTriangleAttributes;

DECLARE_BUFFER(LightmapPathTracerCompute, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Light lights[MAX_LIGHTS];
    EnvProbe envProbes[MAX_ENV_PROBES];
};

#include "../../Include/RayTracing/BVHTraversal.hlsli"

// neighbouring threads hit different materials, so the bindless index must be marked non-uniform
#define SAMPLE_BVH_MATERIAL_TEXTURE(material, name, texcoord) \
    textures[NonUniformResourceIndex((material).texture_indices[(MATERIAL_TEXTURE_##name) / 4][(MATERIAL_TEXTURE_##name) % 4])].SampleLevel(texture_sampler, (texcoord), 0.0)

// Equivalent of LightmapPathTracer.rchit.hlsl for a BVH hit
void ShadeBVHHit(float3 direction, BVHHit hit, inout RayPayload payload)
{
    const BVHTriangleAttributes attributes = bvhTriangleAttributes[hit.triangleIndex];

    const float3 barycentric_coords = float3(1.0 - hit.barycentrics.x - hit.barycentrics.y, hit.barycentrics.x, hit.barycentrics.y);

    float3 normal = normalize(
        UnpackBVHNormal(attributes.packedNormalsMaterialIndex.x) * barycentric_coords.x
        + UnpackBVHNormal(attributes.packedNormalsMaterialIndex.y) * barycentric_coords.y
        + UnpackBVHNormal(attributes.packedNormalsMaterialIndex.z) * barycentric_coords.z);

    const BVHTriangle bvhTriangle = bvhTriangles[hit.triangleIndex];

    float3 geometricNormal = cross(bvhTriangle.edge1.xyz, bvhTriangle.edge2.xyz);

    if (dot(geometricNormal, normal) < 0.0)
    {
        geometricNormal = -geometricNormal;
    }

    const bool hitFromBehind = dot(geometricNormal, direction) > 0.0;

    if (dot(normal, -direction) < 0.0)
    {
        normal = -normal;
    }

    float2 texcoord = attributes.texcoord0Texcoord1.xy * barycentric_coords.x
        + attributes.texcoord0Texcoord1.zw * barycentric_coords.y
        + attributes.texcoord2.xy * barycentric_coords.z;

    const uint material_index = attributes.packedNormalsMaterialIndex.w;

    Material material = (Material)0;

    if (material_index != ~0u)
    {
        material = materials[material_index];
    }

    texcoord *= material.uv_scale;

    float4 material_color = material.albedo;

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    if (HAS_TEXTURE(material, DiffuseMap))
    {
        material_color *= SAMPLE_BVH_MATERIAL_TEXTURE(material, DiffuseMap, float2(texcoord.x, 1.0 - texcoord.y));
    }

    if (HAS_TEXTURE(material, MetalnessMap))
    {
        metalness = SAMPLE_BVH_MATERIAL_TEXTURE(material, MetalnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;
    }

    if (HAS_TEXTURE(material, RoughnessMap))
    {
        roughness = SAMPLE_BVH_MATERIAL_TEXTURE(material, RoughnessMap, float2(texcoord.x, 1.0 - texcoord.y)).r;
    }
#endif // HYP_FEATURES_BINDLESS_TEXTURES

    payload.emissive = float4(GET_MATERIAL_EMISSIVE(material), 1.0);
    payload.throughput = float4(material_color.rgb, metalness); // metalness is stored in the alpha channel
    payload.barycentric_coords = barycentric_coords;
    payload.backFace = (hitFromBehind && !GET_MATERIAL_PARAM_BIT(material, MATERIAL_FLAG_DOUBLE_SIDED)) ? 1u : 0u;
    payload.distance = hit.distance;
    payload.normal = normal;
    payload.roughness = roughness;
}

void TraceScene(float3 origin, float3 direction, float tMin, float tMax, inout RayPayload payload)
{
    BVHHit hit;

    if (TraceBVH(origin, direction, tMin, tMax, false, hit))
    {
        ShadeBVHHit(direction, hit, payload);

        return;
    }

    // same as LightmapPathTracer.rmiss.hlsl
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.barycentric_coords = float3(0.0, 0.0, 0.0);
    payload.backFace = 0;
    payload.distance = -1000.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
}

float CheckInShadow(float3 position, float3 normal, float3 lightDir, float maxDist)
{
    BVHHit hit;

    return float(TraceBVH(position + normal * 0.01, lightDir, 0.1, maxDist > 0.0 ? maxDist : 1000.0, true, hit));
}

float CheckInShadow(float3 position, float3 normal, float3 lightDir)
{
    return CheckInShadow(position, normal, lightDir, -1.0);
}

#include "LightmapIntegrator.hlsli"

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint rayIndex = dispatchThreadId.x;

    if (rayIndex >= rayTracingConstants.numRays)
    {
        return;
    }

    hits[rayIndex] = IntegrateLightmapRay(rayIndex);
}
