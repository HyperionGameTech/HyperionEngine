struct FogVolume
{
    mat4 transformMatrix;
    vec4 aabbMin;
    vec4 aabbMax;
    uvec4 lightIndices[4];

    uint numBoundLights;
    uint _pad0;
    uint _pad1;
    uint _pad2;

    // Must match FogVolumeShaderData in RenderProxy.hpp
    vec4 medium;   // x = density (extinction per metre at full noise), y = forward phase g, z = backward phase g, w = backward share
    vec4 lighting; // rgb = albedo, a = ambient intensity
    vec4 shape;    // x = sun intensity, y = edge fade (metres)
};
