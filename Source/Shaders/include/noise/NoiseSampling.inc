#ifndef HYP_NOISE_SAMPLING_GLSL
#define HYP_NOISE_SAMPLING_GLSL

#include "BlueNoise.inc"

vec3 random_pcg3d(uvec3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    return vec3(v) * (1.0/float(0xffffffffu));
}

#endif