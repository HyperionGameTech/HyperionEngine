#ifndef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
    #error "HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE is not defined"
#endif

#ifdef HYP_DESCRIPTOR_SETS_DECLARE
    #define DESCRIPTOR_SET(index, name) \
        extern DescriptorTable::DeclareSet desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, index, HYP_NAME(name))

    #define DESCRIPTOR_SRV(set_index, slot_index, name) \
        extern DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_SRV, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_UAV(set_index, slot_index, name) \
        extern DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_UAV, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_CBUFF(set_index, slot_index, name) \
        extern DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_CBUFF, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_SSBO(set_index, slot_index, name) \
        extern DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_SSBO, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_ACCELERATION_STRUCTURE(set_index, slot_index, name) \
        extern DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_SAMPLER(set_index, slot_index, name) \
        extern DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_SAMPLER, slot_index, HYP_NAME(name))
#elif defined(HYP_DESCRIPTOR_SETS_DEFINE)
    #define DESCRIPTOR_SET(index, name) \
        DescriptorTable::DeclareSet desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, index, HYP_NAME(name))

    #define DESCRIPTOR_SRV(set_index, slot_index, name) \
        DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_SRV, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_UAV(set_index, slot_index, name) \
        DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_UAV, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_CBUFF(set_index, slot_index, name) \
        DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_CBUFF, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_SSBO(set_index, slot_index, name) \
        DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_SSBO, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_ACCELERATION_STRUCTURE(set_index, slot_index, name) \
        DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE, slot_index, HYP_NAME(name))
    #define DESCRIPTOR_SAMPLER(set_index, slot_index, name) \
        DescriptorTable::DeclareDescriptor desc_##name(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, set_index, DESCRIPTOR_SLOT_SAMPLER, slot_index, HYP_NAME(name))
#endif

#if defined(HYP_DESCRIPTOR_SETS_DECLARE) || defined(HYP_DESCRIPTOR_SETS_DEFINE)

DESCRIPTOR_SET(0, Global);
DESCRIPTOR_SRV(0, 0, GBufferTextures);
DESCRIPTOR_SRV(0, 1, GBufferDepthTexture);
DESCRIPTOR_SRV(0, 2, GBufferMipChain);
DESCRIPTOR_SRV(0, 3, DeferredResult);
DESCRIPTOR_SRV(0, 4, PostFXPreStack);
DESCRIPTOR_SRV(0, 5, PostFXPostStack);
DESCRIPTOR_SRV(0, 6, SSRUVTexture);
DESCRIPTOR_SRV(0, 7, SSRSampleTexture);
DESCRIPTOR_SRV(0, 8, SSRRadiusTexture);
DESCRIPTOR_SRV(0, 9, SSRBlurHorTexture);
DESCRIPTOR_SRV(0, 10, SSRBlurVertTexture);
DESCRIPTOR_SRV(0, 11, SSRFinalTexture);
DESCRIPTOR_SRV(0, 12, EnvProbeTextures);
DESCRIPTOR_SRV(0, 13, PointShadowMaps);
DESCRIPTOR_SRV(0, 14, DepthPyramidResult);
DESCRIPTOR_SRV(0, 15, SSRResult);

DESCRIPTOR_UAV(0, 0, SSRUVImage);
DESCRIPTOR_UAV(0, 1, SSRSampleImage);
DESCRIPTOR_UAV(0, 2, SSRRadiusImage);
DESCRIPTOR_UAV(0, 3, SSRBlurHorImage);
DESCRIPTOR_UAV(0, 5, SSRBlurVertImage);
DESCRIPTOR_UAV(0, 6, VoxelImage);

DESCRIPTOR_SSBO(0, 0, EnvProbesBuffer);

DESCRIPTOR_SAMPLER(0, 0, GBufferDepthSampler);

DESCRIPTOR_SET(1, Scene);
DESCRIPTOR_SET(2, Object);
DESCRIPTOR_SET(3, Material);

#else

#error "What do you mean? I don't understand. What do you want me to do? I don't know what you want me to do."

#endif