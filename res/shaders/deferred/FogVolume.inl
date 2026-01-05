struct FogVolume
{
    mat4 transformMatrix;
    vec4 aabbMin;
    vec4 aabbMax;
    uint numBoundLights;
    uvec4 lightIndices[4];
};