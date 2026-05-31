#ifndef HYP_SKINNING
#define HYP_SKINNING

#define HYP_MAX_BONES 256

// SkeletonsBuffer is presumed to exist and be a StructuredBuffer<float4x4>.

float4x4 CreateSkinningMatrix(uint boneIndicesPacked, float4 boneWeights)
{
    float4x4 skinning = (float4x4)0;

    uint4 boneIndices;
    boneIndices.x = boneIndicesPacked & 0xFF;
    boneIndices.y = (boneIndicesPacked >> 8) & 0xFF;
    boneIndices.z = (boneIndicesPacked >> 16) & 0xFF;
    boneIndices.w = (boneIndicesPacked >> 24) & 0xFF;

    int index0 = min(boneIndices.x, HYP_MAX_BONES - 1);
    skinning += boneWeights.x * SkeletonsBuffer.Load(index0);
    int index1 = min(boneIndices.y, HYP_MAX_BONES - 1);
    skinning += boneWeights.y * SkeletonsBuffer.Load(index1);
    int index2 = min(boneIndices.z, HYP_MAX_BONES - 1);
    skinning += boneWeights.z * SkeletonsBuffer.Load(index2);
    int index3 = min(boneIndices.w, HYP_MAX_BONES - 1);
    skinning += boneWeights.w * SkeletonsBuffer.Load(index3);

    return skinning;
}

#endif // HYP_SKINNING