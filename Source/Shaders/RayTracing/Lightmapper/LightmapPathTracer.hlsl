#define PATHTRACER
#define LIGHTMAPPER

#include "../../include/Defines.hlsli"
#include "../../include/Shared.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/Packing.hlsli"

PERMUTE(MODE, LIGHTMAP, IRRADIANCE, RADIANCE, MOMENTS, BENT_NORMALS);

DECLARE_SAMPLER(LightmapPathTracer, SamplerNearest) SamplerState sampler_nearest;
DECLARE_SAMPLER(LightmapPathTracer, SamplerLinear) SamplerState sampler_linear;

#define texture_sampler sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

DECLARE_SRV(LightmapPathTracer, TLAS) RaytracingAccelerationStructure tlas;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/Scene.hlsli"
#include "../../include/Packing.hlsli"
#include "../../include/Noise.hlsli"
#include "../../include/BRDF.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

/// Blue noise
DECLARE_SRV(LightmapPathTracer, BlueNoiseBuffer) StructuredBuffer<int4> BlueNoiseBuffer;

DECLARE_SRV(LightmapPathTracer, EnvProbesColorTexture) TextureCubeArray envProbesColorTexture;

#include "../../include/BlueNoise.hlsli"
#include "../../include/EnvProbes.hlsli"

DECLARE_SRV(LightmapPathTracer, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

#include "../../include/Octahedron.hlsli"

#include "../../include/RayTracing/RayTracingHelpers.hlsli"
#include "../../include/RayTracing/Payload.hlsli"

DECLARE_UAV(LightmapPathTracer, HitsBuffer) RWStructuredBuffer<float4> hits;

DECLARE_SRV(LightmapPathTracer, RaysBuffer) StructuredBuffer<float4> ray_data;

DECLARE_BUFFER(LightmapPathTracer, CBuffer) cbuffer CBuffer
{
    RayTracingConstants rayTracingConstants;
    Light lights[MAX_LIGHTS];
    EnvProbe envProbes[MAX_ENV_PROBES];
};

void TraceScene(float3 origin, float3 direction, float tMin, float tMax, inout RayPayload payload)
{
    RayDesc rayDesc;
    rayDesc.Origin = origin;
    rayDesc.Direction = direction;
    rayDesc.TMin = tMin;
    rayDesc.TMax = tMax;

    TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 1, 0, rayDesc, payload);
}

#include "LightmapIntegrator.hlsli"

[shader("raygeneration")]
void RayGenMain()
{
    const uint rayIndex = DispatchRaysIndex().x;

    hits[rayIndex] = IntegrateLightmapRay(rayIndex);
}
