#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/noise.inc"
#include "../include/packing.inc"

DECLARE_SRV(RTReflections, GBufferAlbedoTexture) Texture2D gbuffer_albedo_texture;
DECLARE_SRV(RTReflections, GBufferNormalsTexture) Texture2D gbuffer_normals_texture;
DECLARE_SRV(RTReflections, GBufferMaterialTexture) Texture2D<uint2> gbuffer_material_texture;
DECLARE_SRV(RTReflections, GBufferDepthTexture) Texture2D gbuffer_depth_texture;

DECLARE_SAMPLER(RTReflections, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(RTReflections, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

DECLARE_SRV(RTReflections, TLAS) RaytracingAccelerationStructure tlas;
DECLARE_UAV(RTReflections, OutputImage) RWTexture2D<unorm float4> image;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/packing.inc"
#include "../include/noise.inc"
#include "../include/brdf.inc"

/// Blue noise
DECLARE_SRV(RTReflections, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

#include "../include/BlueNoise.inc"
#include "../include/env_probe.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

DECLARE_BUFFER(RTReflections, WorldsBuffer) cbuffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

DECLARE_BUFFER_DYNAMIC(RTReflections, CamerasBuffer) cbuffer CamerasBuffer
{
    Camera camera;
};

#include "../include/rt/RayTracingHelpers.inc"
#include "../include/rt/payload.inc"

DECLARE_BUFFER_DYNAMIC(RTReflections, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
};

#define RAY_OFFSET 0.025
#define NUM_SAMPLES 8

#define USE_MIN_ROUGHNESS 1

[shader("raygeneration")]
void RayGenMain()
{
    const int2 resolution = rayTracingConstants.output_image_resolution;

    const int launch_index = int(DispatchRaysIndex().x);
    const int pixel_index = (launch_index * 2) + int(world_shader_data.frame_counter & 1u);

    if (pixel_index >= resolution.x * resolution.y)
    {
        return;
    }

    const int2 storage_coord = int2(
        pixel_index % resolution.x,
        pixel_index / resolution.x
    );

    const float2 uv = (float2(storage_coord) + 0.5) / float2(resolution);

    const float4x4 view_inverse = camera.invViewMat;
    const float4x4 projection_inverse = camera.invProjMat;

    const float4 normalSample = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_normals_texture, uv, 0.0);
    const float3 normal = GBufferUnpackNormal(normalSample);
    const float depth = SAMPLE_TEXTURE_2D_LOD(sampler_nearest, gbuffer_depth_texture, uv, 0.0).r;
    const float3 position = ReconstructWorldSpacePositionFromDepth(projection_inverse, view_inverse, uv, depth).xyz;

    uint2 gbufferDimensions;
    gbuffer_normals_texture.GetDimensions(gbufferDimensions.x, gbufferDimensions.y);

    const uint2 gbufferCoord = uint2(uv * max(0, int2(gbufferDimensions) - 1));
    const uint2 materialData = gbuffer_material_texture.Load(int3(gbufferCoord, 0)).xy;

    GBufferMaterialParams materialParams;
    GBufferUnpackMaterialParams(normalSample.x, materialData.x, materialParams);

    const float roughness = materialParams.roughness;

#if defined(USE_MIN_ROUGHNESS) && USE_MIN_ROUGHNESS
    if (roughness > rayTracingConstants.min_roughness)
    {
        image[storage_coord] = float4(0.0, 0.0, 0.0, 0.0);

        return;
    }
#endif

    const float3 V = normalize(camera.position.xyz - position);

    const RAY_FLAG flags = RAY_FLAG_FORCE_OPAQUE;
    const float tmin = RAY_OFFSET;
    const float tmax = 1000.0;

    float4 color = float4(0.0, 0.0, 0.0, 0.0);

    float3 tangent;
    float3 bitangent;
    ComputeOrthonormalBasis(normal, tangent, bitangent);

    float2 rnd = float2(
        SampleBlueNoise(storage_coord.x, storage_coord.y, int(world_shader_data.frame_counter % NUM_SAMPLES) * 2, NUM_SAMPLES * 2),
        SampleBlueNoise(storage_coord.x, storage_coord.y, int(world_shader_data.frame_counter % NUM_SAMPLES) * 2 + 1, NUM_SAMPLES * 2)
    );

    float3 H = ImportanceSampleGGX(rnd, normal, roughness);
    H = tangent * H.x + bitangent * H.y + normal * H.z;
    H = normalize(H);

    float3 ray_direction = reflect(-V, H);
    float3 origin = position + normal * RAY_OFFSET;

    RayPayload payload = (RayPayload)0;
    payload.color = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = ray_direction;
    ray.TMin = tmin;
    ray.TMax = tmax;

    TraceRay(tlas, flags, 0xFF, 0, 1, 0, ray, payload);

    if (payload.distance < 0.0)
    {
        image[storage_coord] = float4(0.0, 0.0, 0.0, 0.0);

        return;
    }

    if (payload.distance >= tmin && payload.distance < tmax)
    {
        color += payload.color;
    }

    color = clamp(color, float4(0.0, 0.0, 0.0, 0.0), float4(1.0, 1.0, 1.0, 1.0));

#if defined(USE_MIN_ROUGHNESS) && USE_MIN_ROUGHNESS
    // interpolate alpha based on roughness compared to minimum roughness needed for reflection
    color.a = 1.0 - (roughness / rayTracingConstants.min_roughness);
#endif

    image[storage_coord] = color;
}
