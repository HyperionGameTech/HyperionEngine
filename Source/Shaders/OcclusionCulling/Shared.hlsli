#ifndef HYP_OCCLUSION_CULLING_SHARED
#define HYP_OCCLUSION_CULLING_SHARED

#define HYP_NUM_DEPTH_PYRAMID_OFFSETS 4

#define HYP_DEPTH_PYRAMID_SAMPLE_MAX
// #define HYP_DEPTH_PYRAMID_SAMPLE_MIN

#ifdef HYP_DEPTH_PYRAMID_SAMPLE_MAX
#define HYP_DEPTH_CMP max
#define HYP_DEPTH_CMP_INV min
#define HYP_DEPTHS_INIT float2(0, 100000.0)
#else
#define HYP_DEPTH_CMP min
#define HYP_DEPTH_CMP_INV max
#define HYP_DEPTHS_INIT float2(100000.0, 0)
#endif

static const int2 depth_pyramid_offsets[HYP_NUM_DEPTH_PYRAMID_OFFSETS] = {
    int2(0, 0),
    int2(0, 1),
    int2(1, 1),
    int2(1, 0)
};

uint GetCullBits(float4 pos)
{
    return uint(pos.x < -pos.w)
        | (uint(pos.x > pos.w) << 1)
        | (uint(pos.y < -pos.w) << 2)
        | (uint(pos.y > pos.w) << 3)
        | (uint(pos.z < 0) << 4)
        | (uint(pos.z > pos.w) << 5)
        | (uint(pos.w <= 0) << 6);
}

#endif
