#define DDGI

#include "../../include/defines.inc"
#include "../../include/shared.inc"
#include "../../include/rt/payload.inc"

struct PayloadData
{
    float4 throughput;
    float4 color;
    float4 emissive;
    float distance;
    float3 normal;
    float roughness;
};

[shader("miss")]
void MissMain(inout PayloadData payload)
{
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
}
