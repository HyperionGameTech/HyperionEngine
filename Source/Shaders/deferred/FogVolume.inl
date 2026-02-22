struct FogVolume
{
    mat4 transformMatrix;
    vec4 aabbMin;
    vec4 aabbMax;
    uint numBoundLights;
    uint _pad0;
    uint _pad1;
    uint _pad2;
    uvec4 lightIndices[4];
};