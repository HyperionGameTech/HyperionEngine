#define PATHTRACER
#define LIGHTMAPPER

#include "../../include/defines.inc"
#include "../../include/shared.inc"
#include "../../include/rt/payload.inc"

[shader("miss")]
void MissMain(inout RayPayload payload)
{
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.barycentric_coords = float3(0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
}
