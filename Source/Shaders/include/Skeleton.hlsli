#ifndef HYP_SKELETON
#define HYP_SKELETON

#define HYP_MAX_BONES 256

struct Skeleton
{
    float4x4 bones[HYP_MAX_BONES];
};

float4x4 CreateSkinningMatrix(in Skeleton skeleton, uint boneIndicesPacked, float4 boneWeights)
{
    float4x4 skinning = (float4x4)0;

    uint4 boneIndices;
    boneIndices.x = boneIndicesPacked & 0xFF;
    boneIndices.y = (boneIndicesPacked >> 8) & 0xFF;
    boneIndices.z = (boneIndicesPacked >> 16) & 0xFF;
    boneIndices.w = (boneIndicesPacked >> 24) & 0xFF;

    int index0 = min(boneIndices.x, HYP_MAX_BONES - 1);
    skinning += boneWeights.x * skeleton.bones[index0];
    int index1 = min(boneIndices.y, HYP_MAX_BONES - 1);
    skinning += boneWeights.y * skeleton.bones[index1];
    int index2 = min(boneIndices.z, HYP_MAX_BONES - 1);
    skinning += boneWeights.z * skeleton.bones[index2];
    int index3 = min(boneIndices.w, HYP_MAX_BONES - 1);
    skinning += boneWeights.w * skeleton.bones[index3];

    return skinning;
}

#endif