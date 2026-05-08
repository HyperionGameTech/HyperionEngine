/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/renderers/ShadowRenderer.hpp>

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

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <ShadowRenderer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

// Draw the actual shadowmap
static constexpr uint32 BucketMask = RenderBucketMask<RenderBucket::Opaque, RenderBucket::Translucent, RenderBucket::Lightmapped>;

#pragma region ShadowRendererPassData

ShadowRendererPassData::~ShadowRendererPassData()
{
}

#pragma endregion ShadowRendererPassData

#pragma region ShadowRendererBase

ShadowRendererBase::ShadowRendererBase() = default;

void ShadowRendererBase::Initialize()
{
}

void ShadowRendererBase::Shutdown()
{
    HashSet<CacheKey> cacheKeys;

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

int ShadowRendererBase::RunCleanupCycle(int maxIter)
{
    const uint32 currentFrame = GetFrameCounter();

    int numCycles = RendererBase::RunCleanupCycle(maxIter);

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

void ShadowRendererBase::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.view && renderSetup.world && renderSetup.light);

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
        cachedData->shadowMaps.Resize(lightProxy->numCascades);

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

    Array<RenderProxyList*, FixedAllocator<MaxShadowMapCascades * 2>> renderProxyLists;
    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    for (uint32 cascadeIndex = 0; cascadeIndex < lightProxy->numCascades; cascadeIndex++)
    {
        ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
            light, renderSetup.view, cascadeIndex,
            cachedData->shadowViewsDynamic[cascadeIndex],
            cachedData->shadowViewsStatic[cascadeIndex]);

        cachedData->shadowMaps[cascadeIndex] = shadowMap;

        if (!shadowMap)
        {
            continue;
        }
        
        Camera* camera = cachedData->shadowViewsDynamic[cascadeIndex]->GetCamera();
        AssertDebug(camera != nullptr);

        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(camera));
        if (!cameraProxy)
        {
            // Shadow camera not ready yet.
            // Defer until the next frame.
            continue;
        }

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

        const Mat4f& viewProjMat = cameraProxy->bufferData.viewProjMat;

        FramebufferRef& framebuffer = cachedData->shadowMapFramebuffers[cascadeIndex];

        if (!framebuffer.IsValid())
        {
            const FramebufferDesc& framebufferDesc = cachedData->shadowViewsDynamic[cascadeIndex]->GetViewDesc().framebufferDesc;

            framebuffer = RI.MakeFramebuffer(framebufferDesc);

            uint32 attachmentIndex = 0;

            // initial attachment writes to atlas element
            for (; attachmentIndex < 1; attachmentIndex++)
            {
                const AttachmentDesc& attachmentDesc = framebufferDesc.attachments[attachmentIndex];

                framebuffer->AddAttachment(
                    attachmentIndex,
                    attachmentDesc,
                    shadowMap->GetImageView());
            }

            // remaining attachments - if any - are the framebuffers' own.
            for (; attachmentIndex < framebufferDesc.numAttachments; attachmentIndex++)
            {
                const AttachmentDesc& attachmentDesc = framebufferDesc.attachments[attachmentIndex];

                framebuffer->AddAttachment(
                    attachmentIndex,
                    attachmentDesc);
            }

            CheckResult(framebuffer->Create());
        }

        View* passes[] = {
            // static first so we can copy to cache texture or blit from it
            cachedData->shadowViewsStatic[cascadeIndex],
            cachedData->shadowViewsDynamic[cascadeIndex]
        };

        bool needsClearBeforeDraw = true;

        if (hasBakedStaticShadowMaps)
        {
            needsClearBeforeDraw = false;

            Texture* bakedShadowMap = lightProxy->bakedShadowMap;
            Assert(bakedShadowMap != nullptr);

            ImageSubResource srcImageSubResource {};
            srcImageSubResource.baseArrayLayer = 0;
            srcImageSubResource.numLayers = bakedShadowMap->NumArrayLayers();

            ImageSubResource dstImageSubResource {};
            dstImageSubResource.baseArrayLayer = atlasElement.layerIndex * (shadowMap->GetShadowMapType() == SMT_OMNI ? 6 : 1);
            dstImageSubResource.numLayers = (shadowMap->GetShadowMapType() == SMT_OMNI ? 6 : 1);

            Attachment* depthTarget = framebuffer->GetAttachment(framebuffer->NumAttachments() - 1);
            Assert(depthTarget != nullptr);

            Assert(TextureUtils::BytesPerComponent(depthTarget->GetFormat()) == TextureUtils::BytesPerComponent(bakedShadowMap->GetFormat()));

            frame->cr << InsertBarrier(
                bakedShadowMap->GetGpuImage(),
                RS_COPY_SRC,
                srcImageSubResource);

            frame->cr << InsertBarrier(
                depthTarget->GetGpuImage(),
                RS_COPY_DST,
                dstImageSubResource);

            frame->cr << CopyImage(
                bakedShadowMap->GetGpuImage(),
                depthTarget->GetGpuImage(),
                Vec3u(0, 0, 0),
                Vec3u(atlasElement.offsetCoords.x, atlasElement.offsetCoords.y, 0),
                Vec3u(atlasElement.dimensions.x, atlasElement.dimensions.y, 1),
                srcImageSubResource,
                dstImageSubResource);

            // skip the pass for drawing statics
            passes[0] = nullptr;

            if (passes[1] != nullptr)
            {
                // get it ready for rendering to! (for dynamic shadows)
                frame->cr << InsertBarrier(
                    depthTarget->GetGpuImage(),
                    RS_RENDER_TARGET,
                    dstImageSubResource);
            }
            else
            {
                frame->cr << InsertBarrier(
                    depthTarget->GetGpuImage(),
                    RS_SHADER_RESOURCE,
                    dstImageSubResource);
            }
        }
        else if (cacheStaticShadowMaps)
        {
            // skip rendering static objects if we used the cached texture.

            View* shadowView = cachedData->shadowViewsStatic[cascadeIndex];

            RenderSetup rs = renderSetup.Fork();
            rs.view = shadowView;
            rs.passData = FetchViewPassData(shadowView);
            rs.framebuffer = framebuffer;
            rs.viewport = Viewport { atlasElement.dimensions, Vec2i(atlasElement.offsetCoords) };

            ShadowRendererPassData* pd = DynamicCast<ShadowRendererPassData>(rs.passData);
            AssertDebug(pd != nullptr);

            RenderProxyList& rpl = GetConsumerProxyList(shadowView);
            rpl.BeginRead();
            HYP_DEFER({ rpl.EndRead(); });

            const bool isMatrixDirty = cascadeIndex >= pd->prevCameraMatrices.Size()
                || pd->prevCameraMatrices[cascadeIndex] != viewProjMat;

            if (!isMatrixDirty
                && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
                && !rpl.GetSkeletons().GetDiff().NeedsUpdate())
            {
                // Copy from cached texture over to our atlas
                // NOTE: Not for point light (omni) shadowmaps!

                needsClearBeforeDraw = false;

                Attachment* depthTarget = framebuffer->GetAttachment(framebuffer->NumAttachments() - 1);
                Assert(depthTarget != nullptr);

                Assert(cachedData->cachedShadowMapTexture.IsValid());

                ImageSubResource srcImageSubResource {};
                srcImageSubResource.baseArrayLayer = 0;
                srcImageSubResource.numLayers = cachedData->cachedShadowMapTexture->NumArrayLayers();

                ImageSubResource dstImageSubResource {};
                dstImageSubResource.baseArrayLayer = atlasElement.layerIndex * (shadowMap->GetShadowMapType() == SMT_OMNI ? 6 : 1);
                dstImageSubResource.numLayers = (shadowMap->GetShadowMapType() == SMT_OMNI ? 6 : 1);

                frame->cr << InsertBarrier(
                    cachedData->cachedShadowMapTexture->GetGpuImage(),
                    RS_COPY_SRC,
                    srcImageSubResource);

                frame->cr << InsertBarrier(
                    depthTarget->GetGpuImage(),
                    RS_COPY_DST,
                    dstImageSubResource);

                frame->cr << CopyImage(
                    cachedData->cachedShadowMapTexture->GetGpuImage(),
                    depthTarget->GetGpuImage(),
                    Vec3u(0, 0, 0),
                    Vec3u(atlasElement.offsetCoords.x, atlasElement.offsetCoords.y, 0),
                    Vec3u(atlasElement.dimensions.x, atlasElement.dimensions.y, 1),
                    srcImageSubResource,
                    dstImageSubResource);

                if (!passes[1])
                {
                    frame->cr << InsertBarrier(
                        depthTarget->GetGpuImage(),
                        RS_SHADER_RESOURCE,
                        dstImageSubResource);
                }

                // don't want to render this pass; setting it to null will skip it!
                passes[0] = nullptr;
            }
        }

        for (uint8 passIndex = 0; passIndex < 2; passIndex++)
        {
            Attachment* target = framebuffer->GetAttachment(0);

            GpuImage* resultImage = target->GetGpuImage();
            Assert(resultImage != nullptr);

            View* shadowView = passes[passIndex];

            if (!shadowView)
            {
                continue;
            }

            const bool isStaticShadowMap = passIndex == 0;
            const bool shouldCacheAfterRender = isStaticShadowMap && cacheStaticShadowMaps;

            RenderSetup rs = renderSetup.Fork();
            rs.view = shadowView;
            rs.passData = FetchViewPassData(shadowView);
            rs.framebuffer = framebuffer;
            rs.viewport = Viewport { atlasElement.dimensions, Vec2i(atlasElement.offsetCoords) };

            ShadowRendererPassData* pd = DynamicCast<ShadowRendererPassData>(rs.passData);
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

            if (pd->prevCameraMatrices.Size() <= cascadeIndex)
            {
                pd->prevCameraMatrices.Resize(cascadeIndex + 1);
            }

            pd->prevCameraMatrices[cascadeIndex] = viewProjMat;

            //HYP_LOG(Rendering, Verbose, "Rendering shadows for shadow view {} at frame {}", shadowView->Id(), GetFrameCounter());

            frame->cr << InsertBarrier(
                resultImage,
                RS_RENDER_TARGET,
                target->GetImageView()->GetImageSubResource());

            RenderCollector& renderCollector = GetRenderCollector(shadowView);
            renderCollector.renderGroupFlags &= ~RenderGroupFlags::PARALLEL_RENDERING;
            renderCollector.ExecuteDrawCalls(frame, rs, BucketMask);

            if (shouldCacheAfterRender)
            {
                Assert(cachedData->cachedShadowMapTexture.IsValid());

                // Save rendered result to cache texture

                // need to transition atlas section to COPY_SRC
                frame->cr << InsertBarrier(
                    resultImage,
                    RS_COPY_SRC,
                    target->GetImageView()->GetImageSubResource());

                // and our cache texture should be COPY_DST
                frame->cr << InsertBarrier(
                    cachedData->cachedShadowMapTexture->GetGpuImage(),
                    RS_COPY_DST);

                frame->cr << CopyImage(
                    resultImage,
                    cachedData->cachedShadowMapTexture->GetGpuImage(),
                    Vec3u(atlasElement.offsetCoords.x, atlasElement.offsetCoords.y, 0),
                    Vec3u(0, 0, 0),
                    Vec3u(atlasElement.dimensions.x, atlasElement.dimensions.y, 1),
                    target->GetImageView()->GetImageSubResource(),
                    ImageSubResource {});
            }

            // transition atlas section back to shader read
            frame->cr << InsertBarrier(
                resultImage,
                RS_SHADER_RESOURCE,
                target->GetImageView()->GetImageSubResource());
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

    UpdateGpuData(light);
}

PassData* ShadowRendererBase::CreateViewPassData(View* view, PassDataExt& ext)
{
    ShadowRendererPassData* pd = new ShadowRendererPassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion ShadowRendererBase

} // namespace Hyperion
