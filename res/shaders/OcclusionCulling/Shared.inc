#ifndef HYP_CULL_SHARED_GLSL
#define HYP_CULL_SHARED_GLSL

#define HYP_NUM_DEPTH_PYRAMID_OFFSETS 4

#define HYP_DEPTH_PYRAMID_SAMPLE_MAX
// #define HYP_DEPTH_PYRAMID_SAMPLE_MIN

#ifdef HYP_DEPTH_PYRAMID_SAMPLE_MAX
#define HYP_DEPTH_CMP max
#else
#define HYP_DEPTH_CMP min
#endif

const ivec2 depth_pyramid_offsets[HYP_NUM_DEPTH_PYRAMID_OFFSETS] = {
    ivec2(0, 0),
    ivec2(0, 1),
    ivec2(1, 1),
    ivec2(1, 0)
};

uint GetCullBits(vec4 pos)
{
    return uint(pos.x < -pos.w * 0x01)
        | (uint(pos.x > pos.w) * 0x02)
        | (uint(pos.y < -pos.w) * 0x04)
        | (uint(pos.y > pos.w) * 0x08)
        | (uint(pos.z < -pos.w) * 0x10)
        | (uint(pos.z > pos.w) * 0x20)
        | (uint(pos.w <= 0) * 0x40);
}

#endif