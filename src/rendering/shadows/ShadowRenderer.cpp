/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/shadows/ShadowRenderer.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Texture.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowPassData

ShadowPassData::~ShadowPassData()
{
}

#pragma endregion ShadowPassData

#pragma region ShadowRendererBase

static Handle<FullScreenPass> CreateCombineShadowMapsPass(ShadowMapFilter filterMode, TextureFormat format, Vec2u dimensions, Span<Handle<View>> views)
{
    AssertDebug(views.Size() == 2, "Combine pass requires 2 views (one for static objects, one for dynamic objects)");

    ShaderProperties properties;

    if (filterMode == SMF_VSM)
    {
        properties.Set(NAME("VSM"));
    }

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("CombineShadowMaps"), properties);
    Assert(shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("CombineShadowMapsDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("Src0", views[0]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());
        descriptorSet->SetElement("Src1", views[1]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());
    }

    DeferCreate(descriptorTable);

    Handle<FullScreenPass> combineShadowMapsPass = CreateObject<FullScreenPass>(
        shader,
        descriptorTable,
        format,
        dimensions,
        nullptr);

    combineShadowMapsPass->Create();

    return combineShadowMapsPass;
}

static ComputePipelineRef CreateBlurShadowMapPipeline(const GpuImageViewRef& input, const GpuImageViewRef& output)
{
    Assert(input.IsValid());
    Assert(output.IsValid());

    ShaderRef blurShadowMapShader = g_shaderManager->GetOrCreate(NAME("BlurShadowMap"));
    Assert(blurShadowMapShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = blurShadowMapShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("BlurShadowMapDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("InputTexture", input);
        descriptorSet->SetElement("OutputTexture", output);
    }

    DeferCreate(descriptorTable);

    ComputePipelineRef blurShadowMapPipeline = g_renderBackend->MakeComputePipeline(blurShadowMapShader, descriptorTable);
    DeferCreate(blurShadowMapPipeline);

    return blurShadowMapPipeline;
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
            bool shadowMapFreed = g_renderGlobalState->shadowMapAllocator->FreeShadowMap(shadowMap);
            AssertDebug(shadowMapFreed, "Failed to free shadow map");
        }
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
            HYP_LOG(Rendering, Debug, "Removing cached shadow map for Light {} as it is no longer valid.", it->first.Id());

            if (it->second.shadowMap != nullptr)
            {
                bool shadowMapFreed = g_renderGlobalState->shadowMapAllocator->FreeShadowMap(it->second.shadowMap);
                AssertDebug(shadowMapFreed, "Failed to free shadow map for Light {}!", it->first.Id());
            }

            it = m_cachedShadowMapData.Erase(it);

            continue;
        }

        ++it;
    }

    return numCycles;
}

void ShadowRendererBase::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.light != nullptr);

    Light* light = renderSetup.light;
    ShadowMap* shadowMap = nullptr;

    RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi_GetRenderProxy(light->Id()));
    Assert(lightProxy != nullptr, "Proxy for Light {} not found when rendering shadows!", light->Id());
    Assert(lightProxy->shadowViews.Any(), "Light {} proxy has no shadow view attached!", light->Id());

    Array<Handle<View>> shadowViews = Map(lightProxy->shadowViews, &WeakHandle<View>::Lock);

    // check views validity
    for (const Handle<View>& shadowView : shadowViews)
    {
        Assert(shadowView.IsValid());
        Assert(shadowView->GetOutputTarget().IsValid());
        Assert(shadowView->GetOutputTarget().GetFramebuffer().IsValid());
        Assert(shadowView->GetOutputTarget().GetFramebuffer()->GetAttachment(0) != nullptr);
    }

    auto cacheIt = m_cachedShadowMapData.FindAs(light->Id());

    if (cacheIt == m_cachedShadowMapData.End())
    {
        WeakHandle<Light> lightWeak = light->WeakHandleFromThis();

        shadowMap = AllocateShadowMap(light);
        Assert(shadowMap != nullptr, "Failed to allocate shadow map for Light {}!", light->Id());
        Assert(shadowMap->GetAtlasElement() != nullptr);

        cacheIt = m_cachedShadowMapData.Emplace(lightWeak).first;
        cacheIt->second.shadowMap = shadowMap;

        /// TODO: Better check for using combined pass
        if (lightProxy->shadowViews.Size() == 2)
        {
            cacheIt->second.combineShadowMapsPass = CreateCombineShadowMapsPass(
                shadowMap->GetFilterMode(),
                shadowMap->GetImageView()->GetImage()->GetTextureFormat(), // @TODO get format from Light's settings
                shadowMap->GetAtlasElement()->dimensions,
                shadowViews);

            AssertDebug(cacheIt->second.combineShadowMapsPass->GetExtent() == light->GetShadowMapDimensions());
        }

        if (shadowMap->GetFilterMode() == SMF_VSM)
        {
            const GpuImageViewRef& inputImageView = cacheIt->second.combineShadowMapsPass != nullptr
                ? cacheIt->second.combineShadowMapsPass->GetFinalImageView()
                : shadowViews[0]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView();

            Assert(inputImageView.IsValid());

            /// TODO: Blur into separate texture before blitting to final shadow map,
            /// or we'll end up blurring other maps in the atlas multiple times!
            cacheIt->second.csBlurShadowMap = CreateBlurShadowMapPipeline(inputImageView, shadowMap->GetImageView());
        }

        /// TODO: Add re-alloc of shadow maps if parameters have changed
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

    RenderApi_UpdateGpuData(light->Id());

    const GpuImageRef& shadowMapImage = shadowMap->GetImageView()->GetImage();
    Assert(shadowMapImage.IsValid());
    Assert(atlasElement.layerIndex < shadowMapImage->NumLayers());

    FullScreenPass* combineShadowMapsPass = cacheIt->second.combineShadowMapsPass.Get();
    const ComputePipelineRef& csBlurShadowMap = cacheIt->second.csBlurShadowMap;

    const bool useVsm = shadowMap->GetFilterMode() == SMF_VSM;

    Array<RenderProxyList*> renderProxyLists;
    renderProxyLists.Reserve(shadowViews.Size());

    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    for (const Handle<View>& shadowView : shadowViews)
    {
        const ViewOutputTarget& outputTarget = shadowView->GetOutputTarget();
        Assert(outputTarget.IsValid());

        const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
        Assert(framebuffer.IsValid());

        RenderSetup rs = renderSetup;
        rs.view = shadowView;
        rs.passData = FetchViewPassData(shadowView);

        ShadowPassData* pd = ObjCast<ShadowPassData>(rs.passData);
        AssertDebug(pd != nullptr);

        RenderProxyList& rpl = RenderApi_GetConsumerProxyList(shadowView);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        if (!rpl.GetMeshEntities().GetDiff().NeedsUpdate() && !rpl.GetSkeletons().GetDiff().NeedsUpdate())
        {
            continue;
        }

        RenderCollector& renderCollector = RenderApi_GetRenderCollector(shadowView);
        renderCollector.ExecuteDrawCalls(frame, rs, ((1u << RB_OPAQUE) | (1u << RB_LIGHTMAP) | (1u << RB_TRANSLUCENT)));

        if (!combineShadowMapsPass)
        {
            // blit image into final result
            const GpuImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();
            Assert(framebufferImage.IsValid());

            frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
            frame->renderQueue << InsertBarrier(shadowMapImage, RS_COPY_DST);

            frame->renderQueue << Blit(
                framebufferImage,
                shadowMapImage,
                Rect<uint32> { 0, 0, atlasElement.dimensions.x, atlasElement.dimensions.y },
                Rect<uint32> {
                    atlasElement.offsetCoords.x,
                    atlasElement.offsetCoords.y,
                    atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                    atlasElement.offsetCoords.y + atlasElement.dimensions.y },
                0,                      /* srcMip */
                0,                      /* dstMip */
                0,                      /* srcFace */
                atlasElement.layerIndex /* dstFace */
            );

            frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
            frame->renderQueue << InsertBarrier(shadowMapImage, RS_SHADER_RESOURCE);
        }
    }

    if (combineShadowMapsPass)
    {
        RenderSetup rs = renderSetup;
        // FullScreenPass needs a View set
        rs.view = shadowViews[0];

        // Combine passes into one
        combineShadowMapsPass->Render(frame, rs);

        AttachmentBase* attachment = combineShadowMapsPass->GetFramebuffer()->GetAttachment(0);
        Assert(attachment != nullptr);

        const GpuImageRef& srcImage = attachment->GetImage();
        Assert(srcImage.IsValid());

        // Copy combined shadow map to the final shadow map
        frame->renderQueue << InsertBarrier(srcImage, RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(shadowMapImage, RS_COPY_DST, ImageSubResource { .baseArrayLayer = atlasElement.layerIndex });

        // copy the image
        frame->renderQueue << Blit(
            srcImage,
            shadowMapImage,
            Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
            Rect<uint32> {
                atlasElement.offsetCoords.x,
                atlasElement.offsetCoords.y,
                atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                atlasElement.offsetCoords.y + atlasElement.dimensions.y },
            0,                      /* srcMip */
            0,                      /* dstMip */
            0,                      /* srcFace */
            atlasElement.layerIndex /* dstFace */
        );

        // put the images back into a state for reading
        frame->renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
        frame->renderQueue << InsertBarrier(shadowMapImage, RS_SHADER_RESOURCE, ImageSubResource { .baseArrayLayer = atlasElement.layerIndex });
    }

    if (useVsm)
    {
        AssertDebug(csBlurShadowMap.IsValid());

        struct
        {
            Vec2u imageDimensions;
            Vec2u dimensions;
            Vec2u offset;
        } pushConstants;

        pushConstants.imageDimensions = shadowMapImage->GetExtent().GetXY();
        pushConstants.dimensions = atlasElement.dimensions;
        pushConstants.offset = atlasElement.offsetCoords;

        csBlurShadowMap->SetPushConstants(&pushConstants, sizeof(pushConstants));

        // blur the image using compute shader
        frame->renderQueue << BindComputePipeline(csBlurShadowMap);

        // bind descriptor set containing info needed to blur
        frame->renderQueue << BindDescriptorTable(csBlurShadowMap->GetDescriptorTable(), csBlurShadowMap, {}, frame->GetFrameIndex());

        // put our shadow map in a state for writing
        frame->renderQueue << InsertBarrier(shadowMapImage, RS_UNORDERED_ACCESS, ImageSubResource { .baseArrayLayer = atlasElement.layerIndex });
        frame->renderQueue << DispatchCompute(csBlurShadowMap, Vec3u { (atlasElement.dimensions.x + 7) / 8, (atlasElement.dimensions.y + 7) / 8, 1 });

        // put shadow map back into readable state
        frame->renderQueue << InsertBarrier(shadowMapImage, RS_SHADER_RESOURCE, ImageSubResource { .baseArrayLayer = atlasElement.layerIndex });
    }
}

Handle<PassData> ShadowRendererBase::CreateViewPassData(View* view, PassDataExt& ext)
{
    Handle<ShadowPassData> pd = CreateObject<ShadowPassData>();
    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetViewport();

    return pd;
}

#pragma endregion ShadowRendererBase

#pragma region PointShadowRenderer

ShadowMap* PointShadowRenderer::AllocateShadowMap(Light* light)
{
    return g_renderGlobalState->shadowMapAllocator->AllocateShadowMap(
        ShadowMapType::SMT_OMNI,
        light->GetShadowMapFilter(),
        light->GetShadowMapDimensions());
}

#pragma endregion PointShadowRenderer

#pragma region DirectionalShadowRenderer

ShadowMap* DirectionalShadowRenderer::AllocateShadowMap(Light* light)
{
    return g_renderGlobalState->shadowMapAllocator->AllocateShadowMap(
        ShadowMapType::SMT_DIRECTIONAL,
        light->GetShadowMapFilter(),
        light->GetShadowMapDimensions());
}

#pragma endregion DirectionalShadowRenderer

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray, 1);

} // namespace hyperion
