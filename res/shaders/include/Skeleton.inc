#ifndef HYP_SKELETON_GLSL
#define HYP_SKELETON_GLSL

#define HYP_MAX_BONES 256

struct Skeleton
{
    mat4 bones[HYP_MAX_BONES];
};

#endif