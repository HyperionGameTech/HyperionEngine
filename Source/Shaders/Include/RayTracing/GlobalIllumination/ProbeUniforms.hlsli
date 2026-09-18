#ifndef PROBE_UNIFORMS
#define PROBE_UNIFORMS

#include "../../Defines.hlsli"

#define DDGI_PROBE_SIDE_LENGTH_IRRADIANCE 8
#define DDGI_PROBE_SIDE_LENGTH_DEPTH 8

// must match DDGIMaxCascades in Rendering/RenderProxy.hpp
#define DDGI_MAX_CASCADES 6

struct DDGICascadeData
{
    // lattice coord of the probe stored at local grid position (0, 0, 0); the grid scrolls over the lattice as the camera moves
    ivec4 gridOffset;
    ivec4 gridOffsetPrev;

    vec4 probeSpacing;

    float blendAlpha;
    float rayMaxDistance;
    float normalBias;
    float padding;
};

struct DDGIConstants
{
    mat4 rotationMatrix;

    uvec4 probeBorder;
    uvec4 probeCounts;
    uvec4 gridDimensions;
    uvec4 imageDimensions;

    uint numCascades;
    uint numRaysPerProbe;
    uint numBoundLights;
    uint counter;

    uint cascadeUpdateMask;
    uint cascadeResetMask;
    float probeDistance;
    float padding;

    DDGICascadeData cascades[DDGI_MAX_CASCADES];
};

struct ProbeRayData
{
    vec4 direction_depth;
    vec4 origin;
    vec4 normal;
    vec4 color;
};

#define PROBE_RAY_DATA_INDEX(coord) (coord.x + ddgiConstants.gridDimensions.x * coord.y)

#endif
