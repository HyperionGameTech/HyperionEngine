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

    vec4 _pad3;
};
