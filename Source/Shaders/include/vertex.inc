#ifndef HYP_VERTEX
#define HYP_VERTEX

struct Vertex
{
    float3 position;
    float3 normal;
    float2 texcoord0;
    float2 texcoord1;
    float3 tangent; 
    float3 bitangent;
};

#endif // HYP_VERTEX