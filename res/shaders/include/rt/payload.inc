#ifndef HYP_PAYLOAD
#define HYP_PAYLOAD

#include "../../include/vertex.inc"

struct RayPayload
{
	float4 color;
	float3 normal;
	float distance;
	float roughness;
#if defined(PATHTRACER) || defined(LIGHTMAPPER) || defined(DDGI)
	float4 throughput;
	float4 emissive;
#endif
#ifdef LIGHTMAPPER
	float3 barycentric_coords;
#endif
};

struct RayProbePayload
{
	float3 color;
	float3 normal;
	float distance;
	float rand;
	float3 diffuse;
};

struct RayHit
{
	RayPayload payload;
	float3 position;
};

#ifdef LANG_HLSL

float3 LoadFloat3(ByteAddressBuffer buffer, uint byteOffset)
{
    return float3(
        asfloat(buffer.Load(byteOffset)),
        asfloat(buffer.Load(byteOffset + 4)),
        asfloat(buffer.Load(byteOffset + 8))
    );
}

float2 LoadFloat2(ByteAddressBuffer buffer, uint byteOffset)
{
    return float2(
        asfloat(buffer.Load(byteOffset)),
        asfloat(buffer.Load(byteOffset + 4))
    );
}

/// Expects layout (PACKED)
/// struct Vertex
/// {
///     float3 position;
///     float3 normal;
///	    float2 texcoord0;
/// };
void LoadVertex(ByteAddressBuffer buffer, uint vertexIndex, out Vertex vertex)
{
    uint byteOffset = vertexIndex * 32; // sizeof(Vertex) = 32 bytes

    vertex.position = LoadFloat3(buffer, byteOffset);
    vertex.normal = LoadFloat3(buffer, byteOffset + 12);
    vertex.texcoord0 = LoadFloat2(buffer, byteOffset + 24);
}

#endif // LANG_HLSL

#endif