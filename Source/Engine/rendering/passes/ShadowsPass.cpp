/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/passes/ShadowsPass.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>

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
#include <rendering/RendererMain.hpp>
#include <rendering/TextureViewCache.hpp>

#include <engine/EngineStats.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <ShadowsPass.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statShadowMaps("Rendering/GPU/ShadowMaps");

static constexpr uint32 BucketMask = RenderBucketMask<RenderBucket::Opaque, RenderBucket::Translucent, RenderBucket::Lightmapped>;

#pragma region ShadowsPassData

ShadowsPassData::~ShadowsPassData()
{
}

#pragma endregion ShadowsPassData

#pragma region ShadowsPassBase

ShadowsPassBase::ShadowsPassBase() = default;

void ShadowsPassBase::Initialize()
{
}

void ShadowsPassBase::Shutdown()
{
    TSet<CacheKey> cacheKeys;

    for (KeyValuePair<CacheKey, CachedShadowMapData>& pair : m_cachedShadowMapData)
    {
        cacheKeys.Add(pair.first);
    }

    m_cachedShadowMapData.Clear();

    if (cacheKeys.Any())
    {
        for (CacheKey& cacheKey : cacheKeys)
        {
            bool removed = RI.shadowMapCache->Remove(cacheKey.light, cacheKey.view);

            if (!removed)
            {
                HYP_LOG(Rendering, Warning, "Failed to remove shadow map from cache.");
            }
        }

        cacheKeys.Clear();
    }
}

int ShadowsPassBase::RunCleanupCycle(int maxIter)
{
    const uint32 currentFrame = GetFrameCounter();

    int numCycles = PassBase::RunCleanupCycle(maxIter);

    for (auto it = m_cachedShadowMapData.Begin(); it != m_cachedShadowMapData.End() && numCycles < maxIter; numCycles++)
    {
        CachedShadowMapData& value = it->second;

        if (int64(currentFrame) - int64(value.lastFrameUsed) >= RingBufferDepth)
        {
            HYP_LOG(Rendering, Verbose, "Removing cached shadow map for Light {} + View {} as it has not been used in over {} frames", it->first.light->Id(), it->first.view->Id(), RingBufferDepth);

            bool removed = RI.shadowMapCache->Remove(it->first.light, it->first.view);

            if (!removed)
            {
                HYP_LOG(Rendering, Warning, "Failed to remove shadow map from cache.");
            }

            it = m_cachedShadowMapData.Erase(it);

            continue;
        }

        ++it;
    }

    return numCycles;
}

void ShadowsPassBase::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.view && renderSetup.world && renderSetup.light);

    ENGINE_STAT_GPU_SCOPE(&s_statShadowMaps);

    Light* light = renderSetup.light;

    RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
    Assert(lightProxy != nullptr, "Proxy for Light {} not found when rendering shadows!", light->Id());

    const bool isVarianceShadowMap = light->GetShadowMapFilter() == ShadowMapFilter::SMF_VSM;
    const bool hasBakedStaticShadowMaps = (light->GetLightFlags() & LightFlags::BakeStaticShadows) && lightProxy->bakedShadowMap != nullptr;
    const bool cacheStaticShadowMaps = !hasBakedStaticShadowMaps && (light->GetLightFlags() & LightFlags::CacheStaticShadowMaps);

    CacheKey cacheKey {};
    cacheKey.light = light;
    cacheKey.view = renderSetup.view;

    KeyValuePair<CacheKey, CachedShadowMapData>* existingPair = m_cachedShadowMapData.TryGet(cacheKey);
    CachedShadowMapData* cachedData = existingPair ? &existingPair->second : nullptr;

    if (!cachedData)
    {
        // init shadow data

        cachedData = &m_cachedShadowMapData[cacheKey];

        /*if (isVarianceShadowMap)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                GpuBufferRef& buffer = cachedData->blurUniformBuffers[frameIndex];

                buffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(Vec2u) * 3);

#if HYP_DEBUG_MODE
                buffer->SetDebugName(NAME_FMT("BlurShadowMap_UniformBuffer_Frame{}", frameIndex));
#endif

                CheckResult(buffer->Create());
            }

            /// TODO: Add re-alloc of shadow maps if parameters have changed
        }*/
    }

    cachedData->lastFrameUsed = GetFrameCounter();

    Array<RenderProxyList*, FixedAllocator<6 * 2>> renderProxyLists;
    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    const bool isOmni = lightProxy->light.GetUnsafe()->IsA<PointLight>();

    Span<View*> shadowViewsDynamic;
    Span<View*> shadowViewsStatic;

    RenderProxyCamera* shadowCameraProxy = nullptr;

    for (uint32 cascadeIndex = 0; cascadeIndex < lightProxy->numCascades; cascadeIndex++)
    {
        ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
            light, renderSetup.view,
            cascadeIndex,
            shadowViewsDynamic,
            shadowViewsStatic);

        cachedData->shadowMaps[cascadeIndex] = shadowMap;

        if (cascadeIndex == 0)
        {
            cachedData->shadowViewsDynamic = shadowViewsDynamic;
            cachedData->shadowViewsStatic = shadowViewsStatic;

            Camera* shadowCamera = nullptr;

            if (cachedData->shadowViewsDynamic.Size() > 0)
            {
                shadowCamera = cachedData->shadowViewsDynamic[0]->GetCamera();
            }

            shadowCameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(shadowCamera));
        }

        if (!shadowCameraProxy)
        {
            // Shadow camera not ready yet.
            // Defer until the next frame.
            return;
        }

        if (!shadowMap)
        {
            continue;
        }

        const uint32 numViewsToIterate = (isOmni ? 6 : cascadeIndex + 1);

        for (uint32 viewIndex = cascadeIndex; viewIndex < numViewsToIterate; viewIndex++)
        {
            const bool isFirstCubemapFace = isOmni && viewIndex == 0;

            AssertDebug(shadowMap->GetAtlasElement() != nullptr);

            GpuImage* shadowMapImage = shadowMap->GetImageView()->GetImage();
            AssertDebug(shadowMapImage != nullptr);

            if (cacheStaticShadowMaps && !cachedData->cachedShadowMapTexture)
            {
                TextureDesc textureDesc;
                textureDesc.format = shadowMapImage->GetTextureFormat();
                textureDesc.extent = Vec3u(shadowMap->GetAtlasElement()->dimensions, 1);
                textureDesc.type = shadowMapImage->GetType();
                textureDesc.numLayers = 1;

                cachedData->cachedShadowMapTexture = MakeHandle<Texture>(textureDesc);
                CheckResult(cachedData->cachedShadowMapTexture->Create());
            }
            else if (!cacheStaticShadowMaps && cachedData->cachedShadowMapTexture)
            {
                EnqueueDeletion(std::move(cachedData->cachedShadowMapTexture));
            }

            const ShadowMapAtlasElement& atlasElement = *shadowMap->GetAtlasElement();
            AssertDebug(atlasElement.layerIndex <= UINT8_MAX);
            AssertDebug(atlasElement.layerIndex < shadowMapImage->NumArrayLayers());

            FramebufferRef& framebuffer = cachedData->shadowMapFramebuffers[viewIndex];

            if (!framebuffer.IsValid())
            {
                const FramebufferDesc& framebufferDesc = cachedData->shadowViewsDynamic[viewIndex]->GetViewDesc().framebufferDesc;

                framebuffer = RI.MakeFramebuffer(framebufferDesc);

                uint32 attachmentIndex = 0;

                // initial attachment writes to atlas element
                for (; attachmentIndex < 1; attachmentIndex++)
                {
                    const AttachmentDesc& attachmentDesc = framebufferDesc.attachments[attachmentIndex];

                    GpuImageViewRef imageView = shadowMap->GetImageView();

                    if (isOmni)
                    {
                        // omni -- we want to render into an individual slice
                        const ImageSubResource& subResource = imageView->GetImageSubResource();

                        imageView = RI.MakeImageView(
                            imageView->GetImage(),
                            0,
                            1,
                            subResource.baseArrayLayer + viewIndex,
                            1,
                            TextureType::Texture2D);

                        Assert(imageView.IsValid());

                        CheckResult(imageView->Create());
                    }

                    framebuffer->AddAttachment(attachmentIndex, attachmentDesc, imageView);
                }

                // remaining attachments - if any - are the framebuffers' own.
                for (; attachmentIndex < framebufferDesc.numAttachments; attachmentIndex++)
                {
                    const AttachmentDesc& attachmentDesc = framebufferDesc.attachments[attachmentIndex];

                    // @FIXME: may have issues with new 'slice' omni shadow map rendering?

                    framebuffer->AddAttachment(attachmentIndex, attachmentDesc);
                }

                CheckResult(framebuffer->Create());
            }

            enum : uint8 { ShadowStage_Static, ShadowStage_Dynamic, ShadowStage_Max };

            View* localPasses[ShadowStage_Max] = {
                // static first so we can copy to cache texture or blit from it
                cachedData->shadowViewsStatic[viewIndex],
                cachedData->shadowViewsDynamic[viewIndex]
            };

            bool needsClearBeforeDraw = true;

            if (hasBakedStaticShadowMaps)
            {
                needsClearBeforeDraw = false;

                Texture* bakedShadowMap = lightProxy->bakedShadowMap;
                Assert(bakedShadowMap != nullptr);

                ImageSubResource srcImageSubResource;
                srcImageSubResource.baseArrayLayer = 0;
                srcImageSubResource.numLayers = 1;

                ImageSubResource dstImageSubResource;
                dstImageSubResource.baseArrayLayer = atlasElement.layerIndex;
                dstImageSubResource.numLayers = 1;

                if (isOmni)
                {
                    dstImageSubResource.baseArrayLayer = (atlasElement.layerIndex * 6) + viewIndex;
                    srcImageSubResource.baseArrayLayer = viewIndex;
                }

                Attachment* depthTarget = framebuffer->GetAttachment(framebuffer->NumAttachments() - 1);
                Assert(depthTarget != nullptr);

                Assert(TextureUtils::BytesPerComponent(depthTarget->GetFormat()) == TextureUtils::BytesPerComponent(bakedShadowMap->GetFormat()));

                frame->cr << InsertBarrier(bakedShadowMap->GetGpuImage(), RS_COPY_SRC, srcImageSubResource);
                frame->cr << InsertBarrier(depthTarget->GetGpuImage(), RS_COPY_DST, dstImageSubResource);

                frame->cr << CopyImage(
                    bakedShadowMap->GetGpuImage(),
                    depthTarget->GetGpuImage(),
                    Vec3u(0, 0, 0),
                    Vec3u(atlasElement.offsetCoords.x, atlasElement.offsetCoords.y, 0),
                    Vec3u(atlasElement.dimensions.x, atlasElement.dimensions.y, 1),
                    srcImageSubResource,
                    dstImageSubResource);

                // skip the pass for drawing statics
                localPasses[ShadowStage_Static] = nullptr;

                if (localPasses[ShadowStage_Dynamic] != nullptr)
                {
                    // get it ready for rendering to! (for dynamic shadows)
                    frame->cr << InsertBarrier(depthTarget->GetGpuImage(), RS_RENDER_TARGET, dstImageSubResource);
                }
                else
                {
                    frame->cr << InsertBarrier(depthTarget->GetGpuImage(), RS_SHADER_RESOURCE, dstImageSubResource);
                }
            }
            else if (cacheStaticShadowMaps)
            {
                // skip rendering static objects if we used the cached texture.

                View* shadowView = cachedData->shadowViewsStatic[viewIndex];

                RenderSetup rs = renderSetup.Fork();
                rs.view = shadowView;
                rs.passData = FetchViewPassData(shadowView);
                rs.framebuffer = framebuffer;
                rs.viewport = Viewport { atlasElement.dimensions, Vec2i(atlasElement.offsetCoords) };

                ShadowsPassData* pd = DynamicCast<ShadowsPassData>(rs.passData);
                AssertDebug(pd != nullptr);

                RenderProxyList& rpl = GetConsumerProxyList(shadowView);
                rpl.BeginRead();
                HYP_DEFER({ rpl.EndRead(); });

                const bool isMatrixDirty = viewIndex >= pd->prevCameraMatrices.Size()
                    || pd->prevCameraMatrices[viewIndex] != rpl.cachedViewProjMatrix;

                // Copy from cached
                if (!isMatrixDirty
                    && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
                    && !rpl.GetSkeletons().GetDiff().NeedsUpdate())
                {
                    needsClearBeforeDraw = false;

                    Attachment* depthTarget = framebuffer->GetAttachment(framebuffer->NumAttachments() - 1);
                    Assert(depthTarget != nullptr);

                    Assert(cachedData->cachedShadowMapTexture.IsValid());

                    ImageSubResource srcImageSubResource;
                    srcImageSubResource.baseArrayLayer = 0;
                    srcImageSubResource.numLayers = 1;
                    srcImageSubResource.baseMipLevel = 0;
                    srcImageSubResource.numLevels = 1;

                    ImageSubResource dstImageSubResource;
                    dstImageSubResource.baseArrayLayer = atlasElement.layerIndex;
                    dstImageSubResource.numLayers = 1;
                    dstImageSubResource.baseMipLevel = 0;
                    dstImageSubResource.numLevels = 1;

                    // if omni, copy current face
                    if (isOmni)
                    {
                        srcImageSubResource.baseArrayLayer = viewIndex;
                        dstImageSubResource.baseArrayLayer = (atlasElement.layerIndex * 6) + viewIndex;
                    }

                    frame->cr << InsertBarrier(cachedData->cachedShadowMapTexture->GetGpuImage(), RS_COPY_SRC, srcImageSubResource);
                    frame->cr << InsertBarrier(depthTarget->GetGpuImage(), RS_COPY_DST, dstImageSubResource);

                    frame->cr << CopyImage(
                        cachedData->cachedShadowMapTexture->GetGpuImage(),
                        depthTarget->GetGpuImage(),
                        Vec3u(0, 0, 0),
                        Vec3u(atlasElement.offsetCoords.x, atlasElement.offsetCoords.y, 0),
                        Vec3u(atlasElement.dimensions.x, atlasElement.dimensions.y, 1),
                        srcImageSubResource,
                        dstImageSubResource);

                    if (!localPasses[ShadowStage_Dynamic])
                    {
                        frame->cr << InsertBarrier(depthTarget->GetGpuImage(), RS_SHADER_RESOURCE, dstImageSubResource);
                    }

                    // don't want to draw statics since we used cache; setting it to null will skip it!
                    localPasses[ShadowStage_Static] = nullptr;
                }

                if (pd->prevCameraMatrices.Size() <= viewIndex)
                {
                    pd->prevCameraMatrices.Resize(viewIndex + 1);
                }

                pd->prevCameraMatrices[viewIndex] = rpl.cachedViewProjMatrix;
            }
            else
            {
                // no statics -- everything goes in dynamic
                // so skip it
                localPasses[ShadowStage_Static] = nullptr;
            }

            for (uint8 shadowStage = 0; shadowStage < ShadowStage_Max; shadowStage++)
            {
                Attachment* target = framebuffer->GetAttachment(0);

                GpuImage* resultImage = target->GetGpuImage();
                Assert(resultImage != nullptr);

                View* shadowView = localPasses[shadowStage];

                if (!shadowView)
                {
                    continue;
                }

                AssertDebug(shadowView->GetViewDesc().flags & ViewFlags::SHADOW_VIEW);

                const bool isStaticShadowMap = (shadowStage == ShadowStage_Static);
                const bool shouldCacheAfterRender = isStaticShadowMap && cacheStaticShadowMaps;

                RenderSetup rs = renderSetup.Fork();
                rs.viewport = Viewport { atlasElement.dimensions, Vec2i(atlasElement.offsetCoords) };
                rs.view = shadowView;
                rs.framebuffer = framebuffer;
                rs.passData = FetchViewPassData(shadowView);

                ShadowsPassData* pd = DynamicCast<ShadowsPassData>(rs.passData);
                AssertDebug(pd != nullptr);

                if (needsClearBeforeDraw)
                {
                    Rect<uint32> clearRect {};
                    clearRect.x0 = atlasElement.offsetCoords.x;
                    clearRect.y0 = atlasElement.offsetCoords.y;
                    clearRect.x1 = atlasElement.offsetCoords.x + atlasElement.dimensions.x;
                    clearRect.y1 = atlasElement.offsetCoords.y + atlasElement.dimensions.y;

                    frame->cr << SetCurrentFramebuffer(framebuffer);
                    frame->cr << ClearFramebuffer(framebuffer, clearRect);
                    frame->cr << SetCurrentFramebuffer(nullptr);

                    needsClearBeforeDraw = false;
                }

                RenderProxyList& rpl = GetConsumerProxyList(shadowView);
                rpl.BeginRead();
                renderProxyLists.PushBack(&rpl);

                frame->cr << InsertBarrier(resultImage, RS_RENDER_TARGET, target->GetImageView()->GetImageSubResource());

                RenderCollector& renderCollector = GetRenderCollector(shadowView);
                renderCollector.ExecuteDrawCalls(frame, rs, BucketMask);

                if (shouldCacheAfterRender)
                {
                    Assert(cachedData->cachedShadowMapTexture.IsValid());

                    // Save rendered result to cache texture
                    ImageSubResource srcImageSubResource;
                    srcImageSubResource.baseArrayLayer = atlasElement.layerIndex;
                    srcImageSubResource.numLayers = 1;
                    srcImageSubResource.baseMipLevel = 0;
                    srcImageSubResource.numLevels = 1;

                    ImageSubResource dstImageSubResource;
                    dstImageSubResource.baseArrayLayer = 0;
                    dstImageSubResource.numLayers = 1;
                    dstImageSubResource.baseMipLevel = 0;
                    dstImageSubResource.numLevels = 1;

                    // if omni, copy current face
                    if (isOmni)
                    {
                        srcImageSubResource.baseArrayLayer = (atlasElement.layerIndex * 6) + viewIndex;
                        dstImageSubResource.baseArrayLayer = viewIndex;
                    }

                    // need to transition atlas section to COPY_SRC
                    frame->cr << InsertBarrier(resultImage, RS_COPY_SRC, srcImageSubResource);

                    // and our cache texture should be COPY_DST
                    frame->cr << InsertBarrier(cachedData->cachedShadowMapTexture->GetGpuImage(), RS_COPY_DST, dstImageSubResource);

                    frame->cr << CopyImage(
                        resultImage,
                        cachedData->cachedShadowMapTexture->GetGpuImage(),
                        Vec3u(atlasElement.offsetCoords.x, atlasElement.offsetCoords.y, 0),
                        Vec3u(0, 0, 0),
                        Vec3u(atlasElement.dimensions.x, atlasElement.dimensions.y, 1),
                        srcImageSubResource,
                        dstImageSubResource);
                }

                // transition atlas section back to shader read
                frame->cr << InsertBarrier(resultImage, RS_SHADER_RESOURCE, target->GetImageView()->GetImageSubResource());
            }

#if 0 // FIXME
        if (isVarianceShadowMap)
        {
            AssertDebug(shadowMap != nullptr);

            View* shadowView = shouldCombineShadowMaps
                ? cachedData->shadowViewsStatic[cascadeIndex]
                : cachedData->shadowViewsDynamic[cascadeIndex];

            GpuImageView* inputImageView = cachedData->cachedShadowMapTexture.IsValid()
                ? RI.textureViewCache->GetOrCreate(cachedData->cachedShadowMapTexture)
                : framebuffer->GetAttachment(0)->GetImageView();

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
            cachedData->blurUniformBuffers[frameIndex]->Flush(0, sizeof(uniformData));

            CommandRecorder& cr = frame->cr;

            cr << SetCurrentShader(ShaderDesc(NAME("BlurShadowMap")));

            uint32 numShaderUniforms = 0;

            cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
            cr << SetShaderUniform(numShaderUniforms++, "InputTexture"_sh, inputImageView);
            cr << SetShaderUniform(numShaderUniforms++, "OutputTexture"_sh, outputImageView);
            cr << SetShaderUniform(numShaderUniforms++, "BlurShadowMapUniforms"_sh, cachedData->blurUniformBuffers[frameIndex]);

            // put our shadow map in a state for writing
            cr << InsertBarrier(
                shadowMapImage,
                RS_UNORDERED_ACCESS,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });

            cr << DispatchCompute(Vec3u { (atlasElement.dimensions.x + 7) / 8, (atlasElement.dimensions.y + 7) / 8, 1 });

            // put shadow map back into readable state
            cr << InsertBarrier(
                shadowMapImage,
                RS_SHADER_RESOURCE,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });
        }
#endif
        }
    }

    UpdateGpuData(light);
}

PassData* ShadowsPassBase::CreateViewPassData(View* view, PassDataExt& ext)
{
    ShadowsPassData* pd = new ShadowsPassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion ShadowsPassBase

} // namespace Hyperion
