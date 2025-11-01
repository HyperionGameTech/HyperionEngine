layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

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
#include "../include/env_probe.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SSR_ROUGHNESS_MAX 1.0