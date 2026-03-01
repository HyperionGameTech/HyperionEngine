/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/ShadowRenderer.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>
#include <rendering/shadows/ShadowViewCache.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderCollection.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <ShadowRenderer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowRendererPassData

ShadowRendererPassData::~ShadowRendererPassData()
{
}

#pragma endregion ShadowRendererPassData

#pragma region ShadowRendererBase

static UniquePtr<FullScreenPass> CreateCombineShadowMapsPass(
    ShadowMapFilter filterMode, TextureFormat format, Vec2u dimensions)
{
    ShaderPropertySet properties;

    if (filterMode == SMF_VSM)
    {
        properties.Add(InternShaderProperty(ShaderProperty(NAME("VSM"))));
    }

    UniquePtr<FullScreenPass> combineShadowMapsPass = MakeUnique<FullScreenPass>(
        ShaderDesc(NAME("CombineShadowMaps"), properties),
        format,
        dimensions,
        nullptr);

    combineShadowMapsPass->Create();

    return combineShadowMapsPass;
}

ShadowRendererBase::ShadowRendererBase() = default;

void ShadowRendererBase::Initialize()
{
}

void ShadowRendererBase::Shutdown()
{
    HashSet<ShadowMap*> shadowMaps;

    for (KeyValuePair<CacheKey, CachedShadowMapData>& pair : m_cachedShadowMapData)
    {
        CachedShadowMapData& value = pair.second;

        for (ShadowMap* shadowMap : value.shadowMaps)
        {
            shadowMaps.Insert(shadowMap);
        }
    }

    m_cachedShadowMapData.Clear();

    if (shadowMaps.Any())
    {
        for (ShadowMap* shadowMap : shadowMaps)
        {
            bool removedFromAtlas = g_renderInterface->shadowMapAllocator->FreeShadowMap(shadowMap);

            if (!removedFromAtlas)
            {
                HYP_LOG(Rendering, Warning, "Failed to remove shadow map from atlas.");
            }
        }

        shadowMaps.Clear();
    }
}

int ShadowRendererBase::RunCleanupCycle(int maxIter)
{
    static constexpr uint32 MaxFramesBeforeDiscard = 100;

    const uint32 currentFrame = GetFrameCounter();

    int numCycles = RendererBase::RunCleanupCycle(maxIter);

    for (auto it = m_cachedShadowMapData.Begin(); it != m_cachedShadowMapData.End() && numCycles < maxIter; numCycles++)
    {
        CachedShadowMapData& value = it->second;

        if (currentFrame - value.lastFrameUsed >= MaxFramesBeforeDiscard)
        {
            HYP_LOG(Rendering, Verbose, "Removing cached shadow map for Light {} + View {} as it has not been used in over {} frames", it->first.light->Id(), it->first.view->Id(), MaxFramesBeforeDiscard);

            for (ShadowMap* shadowMap : it->second.shadowMaps)
            {
                bool shadowMapFreed = g_renderInterface->shadowMapAllocator->FreeShadowMap(shadowMap);
                AssertDebug(shadowMapFreed, "Failed to free shadow map for Light {} + View {}", it->first.light->Id(), it->first.view->Id());
            }

            it = m_cachedShadowMapData.Erase(it);

            continue;
        }

        ++it;
    }

    return numCycles;
}

void ShadowRendererBase::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.view && renderSetup.world && renderSetup.light);

    Light* light = renderSetup.light;

    RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
    Assert(lightProxy != nullptr, "Proxy for Light {} not found when rendering shadows!", light->Id());

    const bool isVarianceShadowMap = light->GetShadowMapFilter() == ShadowMapFilter::SMF_VSM;
    const bool shouldCombineShadowMaps = light->GetLightFlags() & LightFlags::ShadowCacheStaticObjects;
    
    CacheKey cacheKey {};
    cacheKey.light = light;
    cacheKey.view = renderSetup.view;

    KeyValuePair<CacheKey, CachedShadowMapData>* existingPair = m_cachedShadowMapData.TryGet(cacheKey);
    CachedShadowMapData* cachedData = existingPair ? &existingPair->second : nullptr;

    if (!cachedData)
    {
        // init shadow data

        cachedData = &m_cachedShadowMapData[cacheKey];
        cachedData->shadowMaps.Resize(lightProxy->numCascades);

        Vec2u maxDimensions;

        for (uint32 cascadeIndex = 0; cascadeIndex < lightProxy->numCascades; cascadeIndex++)
        {
            ShadowMap* shadowMap = AllocateShadowMap(light);
            Assert(shadowMap != nullptr, "Failed to allocate shadow map for Light {} (cascade: {})!", light->Id(), cascadeIndex);
            Assert(shadowMap->GetAtlasElement() != nullptr);
            
            cachedData->shadowMaps[cascadeIndex] = shadowMap;

            maxDimensions = MathUtil::Max(maxDimensions, shadowMap->GetAtlasElement()->dimensions);
        }

        if (shouldCombineShadowMaps)
        {
            cachedData->combineShadowMapsPass = CreateCombineShadowMapsPass(
                isVarianceShadowMap ? ShadowMapFilter::SMF_VSM : ShadowMapFilter::SMF_STANDARD,
                TextureFormat::RG16F,
                maxDimensions);
        }

        if (isVarianceShadowMap)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                GpuBufferRef& buffer = cachedData->blurUniformBuffers[frameIndex];

                buffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(Vec2u) * 3);

#if HYP_DEBUG_MODE
                buffer->SetDebugName(NAME_FMT("BlurShadowMap_UniformBuffer_Frame{}", frameIndex));
#endif

                CheckResult(buffer->Create());
            }

            /// TODO: Add re-alloc of shadow maps if parameters have changed
        }
    }

    cachedData->lastFrameUsed = GetFrameCounter();
    
    FullScreenPass* combineShadowMapsPass = cachedData->combineShadowMapsPass.Get();

    Array<RenderProxyList*, FixedAllocator<MaxShadowMapCascades * 2>> renderProxyLists;
    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    for (uint32 cascadeIndex = 0; cascadeIndex < lightProxy->numCascades; cascadeIndex++)
    {
        cachedData->shadowViewsDynamic[cascadeIndex] = g_renderInterface->shadowViewCache->TryGetShadowView(
            renderSetup.view,
            renderSetup.light,
            cascadeIndex,
            /* isStatic */ false);

        if (!cachedData->shadowViewsDynamic[cascadeIndex])
        {
            continue;
        }
            
        if (shouldCombineShadowMaps)
        {
            cachedData->shadowViewsStatic[cascadeIndex] = g_renderInterface->shadowViewCache->TryGetShadowView(
                renderSetup.view,
                renderSetup.light,
                cascadeIndex,
                /* isStatic */ true);
        }
        else
        {
            cachedData->shadowViewsStatic[cascadeIndex] = nullptr;
        }

        ShadowMap* shadowMap = cachedData->shadowMaps[cascadeIndex];
        Assert(shadowMap != nullptr && shadowMap->GetAtlasElement() != nullptr);
        
        GpuImage* shadowMapImage = shadowMap->GetImageView()->GetImage();
        AssertDebug(shadowMapImage != nullptr);

        const ShadowMapAtlasElement& atlasElement = *shadowMap->GetAtlasElement();
        AssertDebug(atlasElement.layerIndex <= UINT8_MAX);
        AssertDebug(atlasElement.layerIndex < shadowMapImage->NumArrayLayers());

        LightShaderData::ShadowMapCascade& cascadeBufferData = lightProxy->bufferData.cascades[cascadeIndex];
        
        cascadeBufferData.aabbMin.w = atlasElement.offsetUV.x;
        cascadeBufferData.aabbMax.w = atlasElement.offsetUV.y;

        cascadeBufferData.dimensionsScale = Vec4f(Vec2f(atlasElement.dimensions), atlasElement.scale);

        cascadeBufferData.viewProjMat = cachedData->shadowViewsDynamic[cascadeIndex]->GetCamera()->GetViewProjectionMatrix();

        BoundingBox shadowBoundsNDC;
        shadowBoundsNDC.min = Vec3f(-1.0f);
        shadowBoundsNDC.max = Vec3f(1.0f);

        BoundingBox shadowBoundsWS = cascadeBufferData.viewProjMat.Inverse() * shadowBoundsNDC;

        cascadeBufferData.aabbMin.x = shadowBoundsWS.min.x;
        cascadeBufferData.aabbMin.y = shadowBoundsWS.min.y;
        cascadeBufferData.aabbMin.z = shadowBoundsWS.min.z;
        cascadeBufferData.aabbMax.x = shadowBoundsWS.max.x;
        cascadeBufferData.aabbMax.y = shadowBoundsWS.max.y;
        cascadeBufferData.aabbMax.z = shadowBoundsWS.max.z;
        
        lightProxy->bufferData.layerIndices[cascadeIndex] = (atlasElement.layerIndex & 0xFFu);

        for (View* shadowView : { cachedData->shadowViewsDynamic[cascadeIndex], cachedData->shadowViewsStatic[cascadeIndex] })
        {
            if (!shadowView)
            {
                continue;
            }

            const ViewOutputTarget& outputTarget = shadowView->GetOutputTarget();
            AssertDebug(outputTarget.IsValid());

            const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
            AssertDebug(framebuffer.IsValid());

            RenderSetup rs = renderSetup.Fork();
            rs.view = shadowView;
            rs.passData = FetchViewPassData(shadowView);

            ShadowRendererPassData* pd = ObjCast<ShadowRendererPassData>(rs.passData);
            AssertDebug(pd != nullptr);

            RenderProxyList& rpl = GetConsumerProxyList(shadowView);
            rpl.BeginRead();
            renderProxyLists.PushBack(&rpl);

            if (pd->prevCameraMatrices.Size() <= cascadeIndex)
            {
                pd->prevCameraMatrices.Resize(cascadeIndex + 1);
            }

            const bool isMatrixDirty = pd->prevCameraMatrices[cascadeIndex] != cascadeBufferData.viewProjMat;

            if (!isMatrixDirty
                && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
                && !rpl.GetSkeletons().GetDiff().NeedsUpdate())
            {
                continue;
            }

            // @TODO: Octree transforms hash check?

            pd->prevCameraMatrices[cascadeIndex] = cascadeBufferData.viewProjMat;

            // Draw the actual shadowmap
            static constexpr uint32 Mask = RenderBucketMask<RenderBucket::Opaque, RenderBucket::Translucent, RenderBucket::Lightmapped>;

            RenderCollector& renderCollector = GetRenderCollector(shadowView);
            renderCollector.ExecuteDrawCalls(frame, rs, Mask);

            if (!shouldCombineShadowMaps)
            {
                // blit image into final result
                const GpuImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();
                Assert(framebufferImage.IsValid());

                const uint16 numLayers = framebufferImage->NumArrayLayers();

                Assert((atlasElement.layerIndex * numLayers) + numLayers <= shadowMapImage->NumArrayLayers(),
                    "Atlas element has layer index = {} and num faces = {} ({} x {} + {} = {}), but shadow map atlas has total num faces = {}",
                    atlasElement.layerIndex, numLayers,
                    atlasElement.layerIndex, numLayers, numLayers, (atlasElement.layerIndex * numLayers) + numLayers,
                    shadowMapImage->NumArrayLayers());

                ImageSubResource baseSubResource {};
                baseSubResource.baseMipLevel = 0;
                baseSubResource.numLevels = 1;
                baseSubResource.baseArrayLayer = (atlasElement.layerIndex * numLayers);
                baseSubResource.numLayers = numLayers;

                frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
                frame->renderQueue << InsertBarrier(shadowMapImage, RS_COPY_DST, baseSubResource);

                for (uint16 layerIndex = 0; layerIndex < numLayers; layerIndex++)
                {
                    frame->renderQueue << Blit(
                        framebufferImage,
                        shadowMapImage,
                        Rect<uint32> {
                            0, 0,
                            atlasElement.dimensions.x,
                            atlasElement.dimensions.y
                        },
                        Rect<uint32> {
                            atlasElement.offsetCoords.x,
                            atlasElement.offsetCoords.y,
                            atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                            atlasElement.offsetCoords.y + atlasElement.dimensions.y
                        },
                        ImageSubResource {
                            .baseMipLevel = 0,
                            .numLevels = 1,
                            .baseArrayLayer = layerIndex,
                            .numLayers = 1
                        },
                        ImageSubResource {
                            .baseMipLevel = baseSubResource.baseMipLevel,
                            .numLevels = 1,
                            .baseArrayLayer = uint16(baseSubResource.baseArrayLayer + layerIndex),
                            .numLayers = 1
                        });
                }

                frame->renderQueue << InsertBarrier(shadowMapImage, RS_SHADER_RESOURCE, baseSubResource);
                frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
            }
        }

        if (shouldCombineShadowMaps)
        {
            AssertDebug(cachedData->shadowViewsStatic[cascadeIndex]->GetViewDesc().renderTargetDesc.numLayers == 1,
                "Combining static and dynamic shadow maps does not support cubemap targets!");

            RenderSetup rs = renderSetup.Fork();
            // FullScreenPass::Begin() needs a View set
            rs.view = cachedData->shadowViewsStatic[cascadeIndex];

            { // Combine passes into one
                combineShadowMapsPass->Begin(frame, rs);

                frame->renderQueue << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
                frame->renderQueue << SetShaderUniform(5, "Src0"_sh, cachedData->shadowViewsStatic[cascadeIndex]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());
                frame->renderQueue << SetShaderUniform(6, "Src1"_sh, cachedData->shadowViewsDynamic[cascadeIndex]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());

                combineShadowMapsPass->RenderFullScreenQuad(frame, rs);
                combineShadowMapsPass->End(frame, rs);
            }

            AttachmentBase* attachment = combineShadowMapsPass->GetFramebuffer()->GetAttachment(0);
            Assert(attachment != nullptr);

            const GpuImageRef& srcImage = attachment->GetImage();
            Assert(srcImage.IsValid());

            // Copy combined shadow map to the final shadow map
            frame->renderQueue << InsertBarrier(srcImage, RS_COPY_SRC);
            frame->renderQueue << InsertBarrier(
                shadowMapImage,
                RS_COPY_DST,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });

            // copy the image
            frame->renderQueue << Blit(
                srcImage,
                shadowMapImage,
                Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
                Rect<uint32> {
                    atlasElement.offsetCoords.x,
                    atlasElement.offsetCoords.y,
                    atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                    atlasElement.offsetCoords.y + atlasElement.dimensions.y
                },
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = 0,
                    .numLayers = 1
                },
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                }
            );

            // put the images back into a state for reading
            frame->renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
            frame->renderQueue << InsertBarrier(
                shadowMapImage,
                RS_SHADER_RESOURCE,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });
        }
        
        if (isVarianceShadowMap)
        {
            AssertDebug(shadowMap != nullptr);

            View* shadowView = shouldCombineShadowMaps
                ? cachedData->shadowViewsStatic[cascadeIndex]
                : cachedData->shadowViewsDynamic[cascadeIndex];

            GpuImageView* inputImageView = cachedData->combineShadowMapsPass != nullptr
                ? cachedData->combineShadowMapsPass->GetFinalImageView()
                : shadowView->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView();

            GpuImageView* outputImageView = shadowMap->GetImageView();

            AssertDebug(inputImageView != nullptr && outputImageView != nullptr);

            struct alignas(16)
            {
                Vec2u imageDimensions;
                Vec2u dimensions;
                Vec2u offset;
            } uniformData;

            uniformData.imageDimensions = shadowMapImage->GetExtent().GetXY();
            uniformData.dimensions = atlasElement.dimensions;
            uniformData.offset = atlasElement.offsetCoords;

            const uint32 frameIndex = frame->GetFrameIndex();
            cachedData->blurUniformBuffers[frameIndex]->Copy(sizeof(uniformData), &uniformData);

            RenderQueue& rq = frame->renderQueue;

            rq << SetCurrentShader(ShaderDesc(NAME("BlurShadowMap")));

            uint32 numShaderUniforms = 0;

            rq << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
            rq << SetShaderUniform(numShaderUniforms++, "InputTexture"_sh, inputImageView);
            rq << SetShaderUniform(numShaderUniforms++, "OutputTexture"_sh, outputImageView);
            rq << SetShaderUniform(numShaderUniforms++, "BlurShadowMapUniforms"_sh, cachedData->blurUniformBuffers[frameIndex]);

            // put our shadow map in a state for writing
            rq << InsertBarrier(
                shadowMapImage,
                RS_UNORDERED_ACCESS,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });

            rq << DispatchCompute(Vec3u { (atlasElement.dimensions.x + 7) / 8, (atlasElement.dimensions.y + 7) / 8, 1 });

            // put shadow map back into readable state
            rq << InsertBarrier(
                shadowMapImage, 
                RS_SHADER_RESOURCE,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });
        }
    }

    UpdateGpuData(light);
}

PassData* ShadowRendererBase::CreateViewPassData(View* view, PassDataExt& ext)
{
    ShadowRendererPassData* pd = new ShadowRendererPassData();
    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

    return pd;
}

#pragma endregion ShadowRendererBase

#pragma region PointShadowRenderer

ShadowMap* PointShadowRenderer::AllocateShadowMap(Light* light)
{
    return g_renderInterface->shadowMapAllocator->AllocateShadowMap(
        ShadowMapType::SMT_OMNI,
        light->GetShadowMapFilter(),
        light->GetShadowMapDimensions());
}

#pragma endregion PointShadowRenderer

#pragma region DirectionalShadowRenderer

ShadowMap* DirectionalShadowRenderer::AllocateShadowMap(Light* light)
{
    return g_renderInterface->shadowMapAllocator->AllocateShadowMap(
        ShadowMapType::SMT_DIRECTIONAL,
        light->GetShadowMapFilter(),
        light->GetShadowMapDimensions());
}

#pragma endregion DirectionalShadowRenderer

} // namespace Hyperion
