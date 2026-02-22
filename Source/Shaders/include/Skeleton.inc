#ifndef HYP_SKELETON
#define HYP_SKELETON

#define HYP_MAX_BONES 256

struct Skeleton
{
    float4x4 bones[HYP_MAX_BONES];
};

#endif