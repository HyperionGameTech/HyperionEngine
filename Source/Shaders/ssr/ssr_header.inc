#ifndef OUTPUT_FORMAT
#if defined(OUTPUT_RGBA8)
#define OUTPUT_FORMAT rgba8
#define TEMPORAL_BLENDING_GAMMA_CORRECTION
#elif defined(OUTPUT_RGBA16F)
#define OUTPUT_FORMAT rgba16f
#elif defined(OUTPUT_RGBA32F)
#define OUTPUT_FORMAT rgba32f
#else
#define OUTPUT_FORMAT rgba8
#define TEMPORAL_BLENDING_GAMMA_CORRECTION
#endif
#endif

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/defines.inc"
#include "../include/packing.inc"
#include "../include/scene.inc"
#include "../include/shared.inc"
#include "../include/noise.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SSR_ROUGHNESS_MAX 1.0

struct SSRConstants
{
    uint4 dimension;
    float ray_step;
    float num_iterations;
    float max_ray_distance;
    float distance_bias;
    float offset;
    float eye_fade_start;
    float eye_fade_end;
    float screen_edge_fade_start;
    float screen_edge_fade_end;

    float _pad0;
    float _pad1;
    float _pad2;
};
