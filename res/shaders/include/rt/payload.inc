#ifndef HYP_PAYLOAD
#define HYP_PAYLOAD

#include "../../include/vertex.inc"

struct RayPayload
{
	vec4 color;
	vec3 normal;
	float distance;
	float roughness;
#if defined(PATHTRACER) || defined(LIGHTMAPPER) || defined(DDGI)
	vec4 throughput;
	vec4 emissive;
#endif
#ifdef LIGHTMAPPER
	vec3 barycentric_coords;
#endif
};

struct RayProbePayload
{
	vec3 color;
	vec3 normal;
	float distance;
	float rand;
	vec3 diffuse;
};

struct RayHit
{
	RayPayload payload;
	vec3 position;
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