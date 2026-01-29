#ifndef HYP_VERTEX
#define HYP_VERTEX

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texcoord0;
    vec2 texcoord1;
    vec3 tangent; 
    vec3 bitangent;
};

#endif // HYP_VERTEX