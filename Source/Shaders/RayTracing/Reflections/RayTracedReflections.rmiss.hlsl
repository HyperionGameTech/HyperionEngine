#include "../include/RayTracing/Payload.hlsli"

[shader("miss")]
void MissMain(inout RayPayload payload)
{
    payload.color = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
}
