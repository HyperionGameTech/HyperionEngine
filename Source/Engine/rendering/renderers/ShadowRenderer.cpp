/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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
#include <rendering/RenderCollection.hpp>

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
            bool removed = g_renderInterface->shadowMapCache->Remove(cacheKey.light, cacheKey.view);

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
    static constexpr uint32 MaxFramesBeforeDiscard = 100;

    const uint32 currentFrame = GetFrameCounter();

    int numCycles = RendererBase::RunCleanupCycle(maxIter);

    for (auto it = m_cachedShadowMapData.Begin(); it != m_cachedShadowMapData.End() && numCycles < maxIter; numCycles++)
    {
        CachedShadowMapData& value = it->second;

        if (currentFrame - value.lastFrameUsed >= MaxFramesBeforeDiscard)
        {
            HYP_LOG(Rendering, Verbose, "Removing cached shadow map for Light {} + View {} as it has not been used in over {} frames", it->first.light->Id(), it->first.view->Id(), MaxFramesBeforeDiscard);

            bool removed = g_renderInterface->shadowMapCache->Remove(it->first.light, it->first.view);

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
        ShadowMap* shadowMap = g_renderInterface->shadowMapCache->GetShadowMap(
            light, renderSetup.view, cascadeIndex,
            cachedData->shadowViewsDynamic[cascadeIndex],
            cachedData->shadowViewsStatic[cascadeIndex]);
            
        cachedData->shadowMaps[cascadeIndex] = shadowMap;

        if (!shadowMap)
        {
            continue;
        }

        AssertDebug(shadowMap->GetAtlasElement() != nullptr);

        if (shouldCombineShadowMaps && !cachedData->combineShadowMapsPass)
        {
            cachedData->combineShadowMapsPass = CreateCombineShadowMapsPass(
                isVarianceShadowMap ? ShadowMapFilter::SMF_VSM : ShadowMapFilter::SMF_STANDARD,
                TextureFormat::RG16F,
                shadowMap->GetAtlasElement()->dimensions);
        }
        
        GpuImage* shadowMapImage = shadowMap->GetImageView()->GetImage();
        AssertDebug(shadowMapImage != nullptr);

        const ShadowMapAtlasElement& atlasElement = *shadowMap->GetAtlasElement();
        AssertDebug(atlasElement.layerIndex <= UINT8_MAX);
        AssertDebug(atlasElement.layerIndex < shadowMapImage->NumArrayLayers());

        Camera* camera = cachedData->shadowViewsDynamic[cascadeIndex]->GetCamera();
        Assert(camera != nullptr);

        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(camera));
        Assert(cameraProxy != nullptr);

        const Mat4f& viewProjMat = cameraProxy->bufferData.viewProjMat;
        
        FramebufferRef& framebuffer = cachedData->shadowMapFramebuffers[cascadeIndex];

        if (!framebuffer.IsValid())
        {
            const RenderTargetDesc& renderTargetDesc = cachedData->shadowViewsDynamic[cascadeIndex]->GetViewDesc().renderTargetDesc;

            framebuffer = g_renderInterface->MakeFramebuffer(renderTargetDesc);

            uint32 attachmentIndex = 0;

            // initial attachment writes to atlas element
            for (; attachmentIndex < 1; attachmentIndex++)
            {
                const AttachmentDesc& attachmentDesc = renderTargetDesc.attachments[attachmentIndex];

                framebuffer->AddAttachment(
                    attachmentIndex,
                    attachmentDesc,
                    shadowMap->GetImageView());
            }

            // remaining attachments - if any - are the framebuffers' own.
            for (; attachmentIndex < renderTargetDesc.numAttachments; attachmentIndex++)
            {
                const AttachmentDesc& attachmentDesc = renderTargetDesc.attachments[attachmentIndex];

                framebuffer->AddAttachment(
                    attachmentIndex,
                    attachmentDesc);
            }

            CheckResult(framebuffer->Create());
        }

        View* passes[] = {
            cachedData->shadowViewsDynamic[cascadeIndex],
            cachedData->shadowViewsStatic[cascadeIndex]
        };

        for (uint32 passIndex = 0; passIndex < std::size(passes); passIndex++)
        {
            // @TODO check if we need barrier here if > 1 pass - might be automatically inserted in CommitPipelineState().

            View* shadowView = passes[passIndex];

            if (!shadowView)
            {
                continue;
            }

            RenderSetup rs = renderSetup.Fork();
            rs.view = shadowView;
            rs.passData = FetchViewPassData(shadowView);
            rs.framebuffer = framebuffer;
            rs.viewport = Viewport { atlasElement.dimensions, Vec2i(atlasElement.offsetCoords) };

            frame->cr << ClearFramebuffer(framebuffer);

            ShadowRendererPassData* pd = ObjCast<ShadowRendererPassData>(rs.passData);
            AssertDebug(pd != nullptr);

            RenderProxyList& rpl = GetConsumerProxyList(shadowView);
            rpl.BeginRead();
            renderProxyLists.PushBack(&rpl);

            if (pd->prevCameraMatrices.Size() <= cascadeIndex)
            {
                pd->prevCameraMatrices.Resize(cascadeIndex + 1);
            }

            HYP_LOG(Rendering, Verbose, "Rendering shadows for shadow view {} at frame {}", shadowView->Id(), GetFrameCounter());

            const bool isMatrixDirty = pd->prevCameraMatrices[cascadeIndex] != viewProjMat;

            if (!isMatrixDirty
                && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
                && !rpl.GetSkeletons().GetDiff().NeedsUpdate())
            {
                continue;
            }

            Attachment* attachment = framebuffer->GetAttachment(0);

            GpuImage* resultImage = attachment->GetGpuImage();
            Assert(resultImage != nullptr);

            // transition to render target before rendering
            /*frame->cr << InsertBarrier(
                resultImage,
                RS_RENDER_TARGET,
                attachment->GetImageView()->GetImageSubResource());*/

            pd->prevCameraMatrices[cascadeIndex] = viewProjMat;

            GetRenderCollector(shadowView).ExecuteDrawCalls(frame, rs, BucketMask);

            // back to shader read
            frame->cr << InsertBarrier(
                resultImage,
                RS_SHADER_RESOURCE);

#if 0
            if (!shouldCombineShadowMaps)
            {
                // blit directly into final result
                GpuImage* srcImage = framebuffer->GetAttachment(0)->GetGpuImage();
                Assert(srcImage != nullptr);

                const uint16 numLayers = srcImage->NumArrayLayers();

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

                frame->cr << InsertBarrier(srcImage, RS_COPY_SRC);
                frame->cr << InsertBarrier(shadowMapImage, RS_COPY_DST, baseSubResource);

                for (uint16 layerIndex = 0; layerIndex < numLayers; layerIndex++)
                {
                    frame->cr << Blit(
                        srcImage,
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

                frame->cr << InsertBarrier(shadowMapImage, RS_SHADER_RESOURCE, baseSubResource);
                frame->cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
            }
#endif
        }

#if 0
        if (shouldCombineShadowMaps)
        {
            AssertDebug(cachedData->shadowViewsStatic[cascadeIndex]->GetViewDesc().renderTargetDesc.numLayers == 1,
                "Combining static and dynamic shadow maps does not support cubemap targets!");

            RenderSetup rs = renderSetup.Fork();
            // FullScreenPass::Begin() needs a View set
            rs.view = cachedData->shadowViewsStatic[cascadeIndex];

            { // Combine passes into one
                combineShadowMapsPass->Begin(frame, rs);

                Framebuffer* srcFramebuffer0 = cachedData->shadowViewsStatic[cascadeIndex]->GetOutputTarget().GetFramebuffer();
                Attachment* srcAttachment0 = srcFramebuffer0->GetAttachment(srcFramebuffer0->NumAttachments() - 1);

                Framebuffer* srcFramebuffer1 = cachedData->shadowViewsDynamic[cascadeIndex]->GetOutputTarget().GetFramebuffer();
                Attachment* srcAttachment1 = srcFramebuffer1->GetAttachment(srcFramebuffer1->NumAttachments() - 1);

                frame->cr << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
                frame->cr << SetShaderUniform(5, "Src0"_sh, srcAttachment0->GetImageView());
                frame->cr << SetShaderUniform(6, "Src1"_sh, srcAttachment1->GetImageView());

                combineShadowMapsPass->RenderFullScreenQuad(frame, rs);
                combineShadowMapsPass->End(frame, rs);
            }

            AttachmentBase* attachment = combineShadowMapsPass->GetFramebuffer()->GetAttachment(0);
            Assert(attachment != nullptr);

            GpuImage* combineResultImage = attachment->GetGpuImage();
            Assert(combineResultImage != nullptr);

            // Copy combined shadow map to the final shadow map
            frame->cr << InsertBarrier(combineResultImage, RS_COPY_SRC);
            frame->cr << InsertBarrier(
                shadowMapImage,
                RS_COPY_DST,
                ImageSubResource {
                    .baseMipLevel = 0,
                    .numLevels = 1,
                    .baseArrayLayer = uint8(atlasElement.layerIndex),
                    .numLayers = 1
                });

            // copy the image
            frame->cr << Blit(
                combineResultImage,
                shadowMapImage,
                Rect<uint32> { 0, 0, combineResultImage->GetExtent().x, combineResultImage->GetExtent().y },
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
            frame->cr << InsertBarrier(combineResultImage, RS_SHADER_RESOURCE);
            frame->cr << InsertBarrier(
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
        
        if (isVarianceShadowMap)
        {
            AssertDebug(shadowMap != nullptr);

            View* shadowView = shouldCombineShadowMaps
                ? cachedData->shadowViewsStatic[cascadeIndex]
                : cachedData->shadowViewsDynamic[cascadeIndex];

            GpuImageView* inputImageView = cachedData->combineShadowMapsPass != nullptr
                ? cachedData->combineShadowMapsPass->GetFinalImageView()
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

            CommandRecorder& cr = frame->cr;

            cr << SetCurrentShader(ShaderDesc(NAME("BlurShadowMap")));

            uint32 numShaderUniforms = 0;

            cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
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
