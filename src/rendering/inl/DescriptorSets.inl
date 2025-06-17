
HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer, 1, sizeof(BlueNoiseBuffer), false);
HYP_DESCRIPTOR_CBUFF(Global, SphereSamplesBuffer, 1, sizeof(Vec4f) * 4096, false);
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear, 1);
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest, 1);
HYP_DESCRIPTOR_SRV(Global, UITexture, 1);
HYP_DESCRIPTOR_SRV(Global, FinalOutputTexture, 1);
HYP_DESCRIPTOR_SSBO(Global, ObjectsBuffer, 1, ~0u, false); // For instanced objects
HYP_DESCRIPTOR_SRV(Global, VoxelGridTexture, 1);
HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture, 1);
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture, 1);

HYP_DESCRIPTOR_SRV(Object, LightmapVolumeIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Object, LightmapVolumeRadianceTexture, 1);
HYP_DESCRIPTOR_SSBO(Object, CurrentObject, 1, sizeof(EntityShaderData), true); // For non-instanced objects

HYP_DESCRIPTOR_SRV_COND(View, GBufferTextures, num_gbuffer_textures, g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferAlbedoTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferNormalsTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferMaterialTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferLightmapTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferVelocityTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferMaskTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferWSNormalsTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, GBufferTranslucentTexture, 1, !g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV(View, GBufferDepthTexture, 1);
HYP_DESCRIPTOR_SRV(View, GBufferMipChain, 1);
HYP_DESCRIPTOR_SRV(View, DeferredResult, 1);
HYP_DESCRIPTOR_SRV_COND(View, PostFXPreStack, 4, g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV_COND(View, PostFXPostStack, 4, g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported());
HYP_DESCRIPTOR_SRV(View, SSRResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, SSGIResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, SSAOResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, TAAResultTexture, 1);
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, EnvProbeTextures, max_bound_reflection_probes);
HYP_DESCRIPTOR_SRV(View, EnvGridIrradianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, EnvGridRadianceResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, ReflectionProbeResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, DeferredIndirectResultTexture, 1);
HYP_DESCRIPTOR_SRV(View, DepthPyramidResult, 1);

HYP_DESCRIPTOR_SRV_COND(Material, Textures, max_bindless_resources, g_rendering_api->GetRenderConfig().IsBindlessSupported());
HYP_DESCRIPTOR_SRV_COND(Material, Textures, max_bound_textures, !g_rendering_api->GetRenderConfig().IsBindlessSupported());
