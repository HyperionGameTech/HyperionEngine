struct FogVolume
{
    mat4 transformMatrix;
    vec4 aabbMin;
    vec4 aabbMax;
    uint numBoundLights;
    uvec4 lightIndices[4];
};

#define HYP_GET_LIGHT(index) \
    lights[fogVolume.lightIndices[(index / 4)][index % 4]]
