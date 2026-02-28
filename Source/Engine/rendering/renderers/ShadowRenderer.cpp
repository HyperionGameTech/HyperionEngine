/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/ShadowRenderer.hpp>

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

static Handle<FullScreenPass> CreateCombineShadowMapsPass(ShadowMapFilter filterMode, TextureFormat format, Vec2u dimensions, Span<View*> views)
{
    AssertDebug(views.Size() == 2, "Combine pass requires 2 views (one for static objects, one for dynamic objects)");

    ShaderPropertySet properties;

    if (filterMode == SMF_VSM)
    {
        properties.Add(InternShaderProperty(ShaderProperty(NAME("VSM"))));
    }

    Handle<FullScreenPass> combineShadowMapsPass = MakeHandle<FullScreenPass>(
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

    for (const KeyValuePair<WeakHandle<Light>, CachedShadowMapData>& it : m_cachedShadowMapData)
    {
        if (!it.second.shadowMap)
        {
            continue;
        }

        shadowMaps.Insert(it.second.shadowMap);
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
    int numCycles = RendererBase::RunCleanupCycle(maxIter);

    for (auto it = m_cachedShadowMapData.Begin(); it != m_cachedShadowMapData.End() && numCycles < maxIter; numCycles++)
    {
        // check if weak object is no longer alive
        if (!it->first || it->first.GetUnsafe()->GetObjectHeader_Internal()->GetRefCountStrong() == 0)
        {
            HYP_LOG(Rendering, Verbose, "Removing cached shadow map for Light {} as it is no longer valid.", it->first.Id());

            if (it->second.shadowMap != nullptr)
            {
                bool shadowMapFreed = g_renderInterface->shadowMapAllocator->FreeShadowMap(it->second.shadowMap);
                AssertDebug(shadowMapFreed, "Failed to free shadow map for Light {}!", it->first.Id());
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

    AssertDebug(renderSetup.world && renderSetup.light);

    Light* light = renderSetup.light;
    ShadowMap* shadowMap = nullptr;

    RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
    Assert(lightProxy != nullptr, "Proxy for Light {} not found when rendering shadows!", light->Id());
    Assert(lightProxy->shadowViews.Any(), "Light {} proxy has no shadow view attached!", light->Id());

    // check views validity
    for (View* shadowView : lightProxy->shadowViews)
    {
        Assert(shadowView != nullptr);
        Assert(shadowView->GetOutputTarget().IsValid());
        Assert(shadowView->GetOutputTarget().GetFramebuffer().IsValid());
        Assert(shadowView->GetOutputTarget().GetFramebuffer()->GetAttachment(0) != nullptr);
    }

    auto cacheIt = m_cachedShadowMapData.FindAs(light->Id());

    if (cacheIt == m_cachedShadowMapData.End())
    {
        WeakHandle<Light> lightWeak = MakeWeakRef(light);

        shadowMap = AllocateShadowMap(light);
        Assert(shadowMap != nullptr, "Failed to allocate shadow map for Light {}!", light->Id());
        Assert(shadowMap->GetAtlasElement() != nullptr);

        cacheIt = m_cachedShadowMapData.Emplace(lightWeak).first;
        cacheIt->second.shadowMap = shadowMap;

        /// TODO: Better check for using combined pass
        if (lightProxy->shadowViews.Size() == 2)
        {
            GpuImage* image = shadowMap->GetImageView()->GetImage();
            AssertDebug(image != nullptr);

            cacheIt->second.combineShadowMapsPass = CreateCombineShadowMapsPass(
                shadowMap->GetFilterMode(),
                image->GetTextureFormat(), /// \todo get format from Light's settings
                shadowMap->GetAtlasElement()->dimensions,
                lightProxy->shadowViews);

            AssertDebug(cacheIt->second.combineShadowMapsPass->GetExtent() == light->GetShadowMapDimensions());
        }

        if (shadowMap->GetFilterMode() == SMF_VSM)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                cacheIt->second.blurUniformBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(Vec2u) * 3);
                
#if HYP_DEBUG_MODE
                cacheIt->second.blurUniformBuffers[frameIndex]->SetDebugName(NAME_FMT("BlurShadowMap_UniformBuffer_Frame{}", frameIndex));
#endif

                DeferCreate(cacheIt->second.blurUniformBuffers[frameIndex]);
            }

            /// TODO: Add re-alloc of shadow maps if parameters have changed
        }
    }
    else
    {
        shadowMap = cacheIt->second.shadowMap;
    }

    Assert(shadowMap != nullptr);
    Assert(shadowMap->GetAtlasElement() != nullptr);

    const ShadowMapAtlasElement& atlasElement = *shadowMap->GetAtlasElement();

    lightProxy->shadowMap = shadowMap;

    lightProxy->bufferData.dimensionsScale = Vec4f(Vec2f(atlasElement.dimensions), atlasElement.scale);
    lightProxy->bufferData.offsetUv = atlasElement.offsetUv;
    lightProxy->bufferData.layerIndex = atlasElement.layerIndex;

    UpdateGpuData(light);

    const GpuImageRef& shadowMapImage = shadowMap->GetImageView()->GetImage();
    AssertDebug(shadowMapImage.IsValid());
    AssertDebug(atlasElement.layerIndex < shadowMapImage->NumArrayLayers());

    FullScreenPass* combineShadowMapsPass = cacheIt->second.combineShadowMapsPass.Get();

    const bool useVsm = shadowMap->GetFilterMode() == SMF_VSM;

    Array<RenderProxyList*, InlineAllocator<2>> renderProxyLists;
    renderProxyLists.Reserve(lightProxy->shadowViews.Size());

    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    for (uint32 shadowViewIndex = 0; shadowViewIndex < uint32(lightProxy->shadowViews.Size()); shadowViewIndex++)
    {
        View* shadowView = lightProxy->shadowViews[shadowViewIndex];
        AssertDebug(shadowView != nullptr);

        const ViewOutputTarget& outputTarget = shadowView->GetOutputTarget();
        AssertDebug(outputTarget.IsValid());

        const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
        AssertDebug(framebuffer.IsValid());

        RenderSetup rs = renderSetup;
        rs.view = shadowView;
        rs.passData = FetchViewPassData(shadowView);

        ShadowRendererPassData* pd = ObjCast<ShadowRendererPassData>(rs.passData);
        AssertDebug(pd != nullptr);

        RenderProxyList& rpl = GetConsumerProxyList(shadowView);
        rpl.BeginRead();
        renderProxyLists.PushBack(&rpl);

        if (pd->prevCameraMatrices.Size() <= shadowViewIndex)
        {
            pd->prevCameraMatrices.Resize(shadowViewIndex + 1);
        }
        
        const bool isMatrixDirty = pd->prevCameraMatrices[shadowViewIndex] != lightProxy->bufferData.shadowMatrix;

        if (!isMatrixDirty
            && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
            && !rpl.GetSkeletons().GetDiff().NeedsUpdate())
        {
            continue;
        }

        // @TODO: Octree transforms hash check?

        pd->prevCameraMatrices[shadowViewIndex] = lightProxy->bufferData.shadowMatrix;

        RenderCollector& renderCollector = GetRenderCollector(shadowView);
        renderCollector.ExecuteDrawCalls(frame, rs, ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT) | (1u << RB_LIGHTMAP)));

        if (!combineShadowMapsPass)
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
                    Rect<uint32> { 0, 0, atlasElement.dimensions.x, atlasElement.dimensions.y },
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
                    }
                );
            }

            frame->renderQueue << InsertBarrier(shadowMapImage, RS_SHADER_RESOURCE, baseSubResource);
            frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
        }
    }

    if (combineShadowMapsPass)
    {
        AssertDebug(lightProxy->shadowViews[0]->GetViewDesc().renderTargetDesc.numLayers == 1,
            "Combining static and dynamic shadow maps does not support cubemap targets!");

        RenderSetup rs = renderSetup;
        // FullScreenPass::Begin() needs a View set
        rs.view = lightProxy->shadowViews[0];

        { // Combine passes into one
            combineShadowMapsPass->Begin(frame, rs);

            frame->renderQueue << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
            frame->renderQueue << SetShaderUniform(5, "Src0"_sh, lightProxy->shadowViews[0]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());
            frame->renderQueue << SetShaderUniform(6, "Src1"_sh, lightProxy->shadowViews[1]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());

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

    if (useVsm)
    {
        const GpuImageViewRef& inputImageView = cacheIt->second.combineShadowMapsPass != nullptr
            ? cacheIt->second.combineShadowMapsPass->GetFinalImageView()
            : lightProxy->shadowViews[0]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView();

        Assert(inputImageView.IsValid());

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
        cacheIt->second.blurUniformBuffers[frameIndex]->Copy(sizeof(uniformData), &uniformData);

        RenderQueue& rq = frame->renderQueue;

        rq << SetCurrentShader(ShaderDesc(NAME("BlurShadowMap")));

        uint32 numShaderUniforms = 0;

        rq << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        rq << SetShaderUniform(numShaderUniforms++, "InputTexture"_sh, inputImageView);
        rq << SetShaderUniform(numShaderUniforms++, "OutputTexture"_sh, shadowMap->GetImageView());
        rq << SetShaderUniform(numShaderUniforms++, "BlurShadowMapUniforms"_sh, cacheIt->second.blurUniformBuffers[frameIndex]);

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
