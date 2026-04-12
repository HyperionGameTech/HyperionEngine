#define PATHTRACER

#include "../../include/Defines.hlsli"
#include "../../include/Shared.hlsli"
#include "../../include/rt/Payload.hlsli"

[shader("miss")]
void MissMain(inout RayPayload payload)
{
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
}
