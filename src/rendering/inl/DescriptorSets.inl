#ifndef HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE
    #error "HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE is not defined"
#endif

#define HYP_DESCRIPTOR_SET(index, name) \
    static DescriptorTableDeclaration::DeclareSet HYP_UNIQUE_NAME(DescriptorSet_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, index, HYP_NAME_UNSAFE(name))

#define HYP_DESCRIPTOR_SRV_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SRV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_UAV_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_UAV, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_CBUFF_COND(set_name, name, count, size, is_dynamic, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_CBUFF, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, is_dynamic)
#define HYP_DESCRIPTOR_SSBO_COND(set_name, name, count, size, is_dynamic, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SSBO, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count, size, is_dynamic)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)
#define HYP_DESCRIPTOR_SAMPLER_COND(set_name, name, count, cond) \
    static DescriptorTableDeclaration::DeclareDescriptor HYP_UNIQUE_NAME(Descriptor_##name)(HYP_DESCRIPTOR_SETS_GLOBAL_STATIC_DESCRIPTOR_TABLE, HYP_NAME_UNSAFE(set_name), DESCRIPTOR_SLOT_SAMPLER, HYP_NAME_UNSAFE(name), HYP_MAKE_CONST_ARG(cond), count)

#define HYP_DESCRIPTOR_SRV(set_name, name, count) HYP_DESCRIPTOR_SRV_COND(set_name, name, count, true)
#define HYP_DESCRIPTOR_UAV(set_name, name, count) HYP_DESCRIPTOR_UAV_COND(set_name, name, count, true)
#define HYP_DESCRIPTOR_CBUFF(set_name, name, count, size, is_dynamic) HYP_DESCRIPTOR_CBUFF_COND(set_name, name, count, size, is_dynamic, true)
#define HYP_DESCRIPTOR_SSBO(set_name, name, count, size, is_dynamic) HYP_DESCRIPTOR_SSBO_COND(set_name, name, count, size, is_dynamic, true)
#define HYP_DESCRIPTOR_ACCELERATION_STRUCTURE(set_name, name, count) HYP_DESCRIPTOR_ACCELERATION_STRUCTURE_COND(set_name, name, count, true)
#define HYP_DESCRIPTOR_SAMPLER(set_name, name, count) HYP_DESCRIPTOR_SAMPLER_COND(set_name, name, count, true)

HYP_DESCRIPTOR_SET(0, Global);
HYP_DESCRIPTOR_SRV(Global, GBufferTextures, num_gbuffer_textures);
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture, 1);
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain, 1);
HYP_DESCRIPTOR_SRV(Global, DeferredResult, 1);
HYP_DESCRIPTOR_SRV(Global, PostFXPreStack, 4);
HYP_DESCRIPTOR_SRV(Global, PostFXPostStack, 4);
HYP_DESCRIPTOR_CBUFF(Global, PostProcessingUniforms, 1, sizeof(PostProcessingUniforms), false);
HYP_DESCRIPTOR_SRV(Global, SSRResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, SSAOResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, TAAResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, EnvGridIrradianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, EnvGridRadianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, ReflectionProbeResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DeferredIndirectResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DeferredDirectResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DepthPyramidResult, 1);
HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer, 1, sizeof(BlueNoiseBuffer), false);
HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, 1, sizeof(DDGIUniforms), false);
HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture, 1);
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear, 1);
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest, 1);
HYP_DESCRIPTOR_SRV(Global, UITexture, 1);
HYP_DESCRIPTOR_SRV(Global, FinalOutputTexture, 1);

HYP_DESCRIPTOR_SET(1, Scene);
HYP_DESCRIPTOR_SSBO(Scene, ScenesBuffer, 1, sizeof(SceneShaderData), true);
HYP_DESCRIPTOR_SSBO(Scene, LightsBuffer, 1, sizeof(LightShaderData), true);
HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, 1, sizeof(EntityShaderData) * max_entities, false);
HYP_DESCRIPTOR_CBUFF(Scene, CamerasBuffer, 1, sizeof(CameraShaderData), true);
HYP_DESCRIPTOR_CBUFF(Scene, EnvGridsBuffer, 1, sizeof(EnvGridShaderData), true);
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, 1, sizeof(EnvProbeShaderData) * max_env_probes, false);
HYP_DESCRIPTOR_SSBO(Scene, CurrentEnvProbe, 1, sizeof(EnvProbeShaderData), true);
HYP_DESCRIPTOR_SSBO(Scene, ShadowMapsBuffer, 1, sizeof(ShadowShaderData) * max_shadow_maps, false);
HYP_DESCRIPTOR_SRV(Scene, ShadowMapTextures, max_shadow_maps);
HYP_DESCRIPTOR_SRV(Scene, PointLightShadowMapTextures, max_bound_point_shadow_maps);
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, max_bound_reflection_probes);
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer, 1, sizeof(SHGridBuffer), false);
HYP_DESCRIPTOR_SRV(Scene, VoxelGridTexture, 1);

HYP_DESCRIPTOR_SET(2, Object);
HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData) * max_materials, false, !renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial());
HYP_DESCRIPTOR_SSBO_COND(Object, MaterialsBuffer, 1, sizeof(MaterialShaderData), true, renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial());
HYP_DESCRIPTOR_SSBO(Object, SkeletonsBuffer, 1, sizeof(SkeletonShaderData), true);

HYP_DESCRIPTOR_SET(3, Material);
HYP_DESCRIPTOR_SRV_COND(Material, Textures, max_bindless_resources, renderer::RenderConfig::IsBindlessSupported());
HYP_DESCRIPTOR_SRV_COND(Material, Textures, max_bound_textures, !renderer::RenderConfig::IsBindlessSupported());

#undef HYP_DESCRIPTOR_SET
#undef HYP_DESCRIPTOR_SRV
#undef HYP_DESCRIPTOR_UAV
#undef HYP_DESCRIPTOR_CBUFF
#undef HYP_DESCRIPTOR_SSBO
#undef HYP_DESCRIPTOR_ACCELERATION_STRUCTURE
#undef HYP_DESCRIPTOR_SAMPLER
