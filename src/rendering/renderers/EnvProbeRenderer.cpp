/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderGpuImageView.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <HyperionEngine.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <EnvProbeRenderer.generated.inl>

namespace hyperion {

static constexpr Vec2u ShNumSamples = { 16, 16 };
static constexpr Vec2u ShNumTiles = { 16, 16 };
static constexpr uint32 ShNumLevels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(ShNumSamples.Max()) + 1));
static constexpr bool ShParallelReduce = false;

static FixedArray<Mat4f, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Mat4f, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Mat4f::LookAt(
            origin,
            origin + Texture::s_cubemapDirections[i].first,
            Texture::s_cubemapDirections[i].second);
    }

    return viewMatrices;
}

#pragma region EnvProbeRenderer

EnvProbeRenderer::EnvProbeRenderer()
{
}

EnvProbeRenderer::~EnvProbeRenderer()
{
}

void EnvProbeRenderer::Initialize()
{
}

void EnvProbeRenderer::Shutdown()
{
}

void EnvProbeRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.envProbe != nullptr);

    EnvProbe* envProbe = renderSetup.envProbe;
    AssertDebug(envProbe != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = envProbe->GetView().Get();
    rs.passData = FetchViewPassData(rs.view);
    rs.envProbe = renderSetup.prev ? renderSetup.prev->envProbe : nullptr;

    RenderProbe(frame, rs, envProbe);
}

Handle<PassData> EnvProbeRenderer::CreateViewPassData(View* view, PassDataExt& ext)
{
    Handle<EnvProbeRendererPassData> pd = CreateObject<EnvProbeRendererPassData>();
    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

    return pd;
}

#pragma endregion EnvProbeRenderer

#pragma region ReflectionProbeRenderer

ReflectionProbeRenderer::ReflectionProbeRenderer()
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Initialize()
{
    HYP_SCOPE;

    EnvProbeRenderer::Initialize();
}

void ReflectionProbeRenderer::Shutdown()
{
    HYP_SCOPE;

    EnvProbeRenderer::Shutdown();
}

void ReflectionProbeRenderer::RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    EnvProbeRendererPassData* pd = ObjCast<EnvProbeRendererPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // special checks for Sky + caching result based on light position + intensity
    if (envProbe->IsA(SkyProbe::StaticClass()))
    {
        if (!renderSetup.light)
        {
            HYP_LOG(Rendering, Warning, "No directional light bound while rendering SkyProbe {} in view {}", envProbe->Id(), view->Id());

            pd->cachedLightDirIntensity = MathUtil::NaN<Vec4f>();

            return;
        }

        if (renderSetup.light->GetLightType() != LT_DIRECTIONAL)
        {
            HYP_LOG(Rendering, Warning, "Light bound to SkyProbe pass is not a directional light: {} in view {}",
                renderSetup.light->Id(), view->Id());

            pd->cachedLightDirIntensity = MathUtil::NaN<Vec4f>();

            return;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi::GetRenderProxy(renderSetup.light));
        AssertDebug(lightProxy != nullptr);
        AssertDebug(RenderApi::RetrieveResourceBinding(renderSetup.light) != ~0u);

        if (lightProxy->bufferData.positionIntensity == pd->cachedLightDirIntensity
            && !rpl.GetMeshEntities().GetDiff().NeedsUpdate())
        {
            // no need to render it just yet if values have not changed -- return early
            return;
        }

        // cache it to save on rendering later
        pd->cachedLightDirIntensity = lightProxy->bufferData.positionIntensity;
    }

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi::GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    if (!rpl.GetMeshEntities().GetDiff().NeedsUpdate()
        && !rpl.GetLights().GetDiff().NeedsUpdate()
        && pd->cachedProbeOrigin == envProbeProxy->bufferData.worldPosition.GetXYZ())
    {
        return;
    }

    pd->cachedProbeOrigin = envProbeProxy->bufferData.worldPosition.GetXYZ();

    RenderCollector& renderCollector = RenderApi::GetRenderCollector(view);

    HYP_LOG(Rendering, Info, "Render EnvProbe {} with {} mesh entities (shared: {}), num total draw calls: {}", envProbe->Id(), rpl.GetMeshEntities().NumCurrent(),
        rpl.isShared,
        renderCollector.NumDrawCallsCollected());

    renderCollector.ExecuteDrawCalls(frame, renderSetup, ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const GpuImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();

    if (envProbe->ShouldComputePrefilteredEnvMap())
    {
        ComputePrefilteredEnvMap(frame, renderSetup, envProbe);
    }

    if (envProbe->ShouldComputeSphericalHarmonics())
    {
        ComputeSH(frame, renderSetup, envProbe);
    }

    if (SkyProbe* skyProbe = ObjCast<SkyProbe>(envProbe))
    {
        Assert(skyProbe->GetSkyboxCubemap().IsValid());

        const GpuImageRef& dstImage = skyProbe->GetSkyboxCubemap()->GetGpuImage();
        Assert(dstImage.IsValid());
        Assert(dstImage->IsCreated());

        frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(dstImage, RS_COPY_DST);

        frame->renderQueue << Blit(framebufferImage, dstImage);

        if (dstImage->HasMipmaps())
        {
            frame->renderQueue << GenerateMipmaps(dstImage);
        }

        frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
        frame->renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }
}

void ReflectionProbeRenderer::ComputePrefilteredEnvMap(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi::GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    struct ConvolveProbeUniforms
    {
        Vec2u outImageDimensions;
        Vec4f worldPosition;
        uint32 numBoundLights;
        alignas(16) uint32 lightIndices[16];
    };

    ShaderProperties shaderProperties;
    shaderProperties.Set(ShaderProperty(NAME("LOBE_SIZE"), 0.94f));
    shaderProperties.Set(ShaderProperty(NAME("NUM_SAMPLES"), 16));

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Set(ShaderProperty(NAME("LIGHTING")));
    }

    ShaderRef convolveProbeShader = g_shaderManager->GetOrCreate(NAME("ConvolveProbe"), shaderProperties);

    if (!convolveProbeShader)
    {
        HYP_FAIL("Failed to create ConvolveProbe shader");
    }

    const Handle<Texture>& prefilteredEnvMap = envProbe->GetPrefilteredEnvMap();
    Assert(prefilteredEnvMap.IsValid());

    ConvolveProbeUniforms uniforms;
    uniforms.outImageDimensions = prefilteredEnvMap->GetExtent().GetXY();
    uniforms.worldPosition = envProbeProxy->bufferData.worldPosition;

    const uint32 maxBoundLights = ArraySize(uniforms.lightIndices);
    uint32 numBoundLights = 0;

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
        {
            continue;
        }

        if (numBoundLights >= maxBoundLights)
        {
            break;
        }

        uniforms.lightIndices[numBoundLights++] = RenderApi::RetrieveResourceBinding(light);
    }

    uniforms.numBoundLights = numBoundLights;

    GpuBufferRef uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
    Assert(uniformBuffer->Create());
    uniformBuffer->Copy(sizeof(uniforms), &uniforms);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* momentsAttachment = framebuffer->GetAttachment(2);

    AssertDebug(colorAttachment != nullptr);
    AssertDebug(normalsAttachment != nullptr);
    AssertDebug(momentsAttachment != nullptr);

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(convolveProbeShader->GetCompiledShader()->GetDescriptorTableDeclaration());
    descriptorTable->SetDebugName(NAME_FMT("ConvolveProbeDescriptorTable_{}", envProbe->Id().Value()));

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ConvolveProbeDescriptorSet", frameIndex);
        AssertDebug(descriptorSet != nullptr);

        descriptorSet->SetElement("UniformBuffer", uniformBuffer);
        descriptorSet->SetElement("ColorTexture", colorAttachment->GetImageView());
        descriptorSet->SetElement("NormalsTexture", normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement("MomentsTexture", momentsAttachment ? momentsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement("SamplerLinear", g_renderGlobalState->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement("SamplerNearest", g_renderGlobalState->placeholderData->GetSamplerNearest());
        descriptorSet->SetElement("OutImage", g_renderBackend->GetTextureImageView(prefilteredEnvMap));
    }

    Assert(descriptorTable->Create());

    ComputePipelineRef convolveProbeComputePipeline = g_renderBackend->MakeComputePipeline(convolveProbeShader, descriptorTable);
    Assert(convolveProbeComputePipeline->Create());

    frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->renderQueue << BindComputePipeline(convolveProbeComputePipeline);

    frame->renderQueue << BindDescriptorTable(
        descriptorTable,
        convolveProbeComputePipeline,
        { { "Global", { { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(
        convolveProbeComputePipeline,
        Vec3u { (prefilteredEnvMap->GetExtent().x + 7) / 8, (prefilteredEnvMap->GetExtent().y + 7) / 8, 1 });

    if (prefilteredEnvMap->GetTextureDesc().HasMipmaps())
    {
        frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_COPY_DST);
        frame->renderQueue << GenerateMipmaps(prefilteredEnvMap->GetGpuImage());
    }

    frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_SHADER_RESOURCE);

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = frame->OnFrameEnd.Bind([delegateHandle, uniformBuffer = std::move(uniformBuffer), convolveProbeComputePipeline = std::move(convolveProbeComputePipeline), descriptorTable = std::move(descriptorTable)](...) mutable
        {
            SafeDelete(std::move(uniformBuffer));
            SafeDelete(std::move(convolveProbeComputePipeline));
            SafeDelete(std::move(descriptorTable));

            delete delegateHandle;
        });
}

void ReflectionProbeRenderer::ComputeSH(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi::GetRenderProxy(envProbe));
    Assert(envProbeProxy != nullptr);

    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const ViewOutputTarget& outputTarget = envProbe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    Assert(colorAttachment != nullptr);

    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* depthAttachment = framebuffer->GetAttachment(2);

    Array<GpuBufferRef, FixedAllocator<ShNumLevels>> shTilesBuffers;
    shTilesBuffers.Resize(ShNumLevels);

    Array<DescriptorTableRef, FixedAllocator<ShNumLevels>> shTilesDescriptorTables;
    shTilesDescriptorTables.Resize(ShNumLevels);

    for (uint32 i = 0; i < ShNumLevels; i++)
    {
        const SizeType size = sizeof(SHTile) * (ShNumTiles.x >> i) * (ShNumTiles.y >> i);

        shTilesBuffers[i] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, size);
        shTilesBuffers[i]->SetRequireCpuAccessible(true);
        Assert(shTilesBuffers[i]->Create());
    }

    ShaderProperties shaderProperties;

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Set(ShaderProperty(NAME("LIGHTING")));
    }

    enum
    {
        MODE_CLEAR,
        MODE_BUILD_COEFFICIENTS,
        MODE_REDUCE,
        MODE_FINALIZE,

        MODE_MAX
    };

    FixedArray<Pair<ShaderRef, ComputePipelineRef>, MODE_MAX> pipelines = {
        Pair<ShaderRef, ComputePipelineRef> { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { ShaderProperty(NAME("MODE"), NAME("CLEAR")) } })), ComputePipelineRef::Null() },
        Pair<ShaderRef, ComputePipelineRef> { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { ShaderProperty(NAME("MODE"), NAME("BUILD_COEFFICIENTS")) } })), ComputePipelineRef::Null() },
        Pair<ShaderRef, ComputePipelineRef> { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { ShaderProperty(NAME("MODE"), NAME("REDUCE")) } })), ComputePipelineRef::Null() },
        Pair<ShaderRef, ComputePipelineRef> { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { ShaderProperty(NAME("MODE"), NAME("FINALIZE")) } })), ComputePipelineRef::Null() }
    };

    ShaderRef firstShader;

    for (auto& it : pipelines)
    {
        if (!firstShader)
        {
            firstShader = it.first;
        }
    }

    const DescriptorTableDeclaration* descriptorTableDecl = firstShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    Array<DescriptorTableRef, FixedAllocator<ShNumLevels>> computeShDescriptorTables;
    computeShDescriptorTables.Resize(ShNumLevels);

    for (uint32 i = 0; i < ShNumLevels; i++)
    {
        computeShDescriptorTables[i] = g_renderBackend->MakeDescriptorTable(descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            const DescriptorSetRef& computeShDescriptorSet = computeShDescriptorTables[i]->GetDescriptorSet("ComputeSHDescriptorSet", frameIndex);
            Assert(computeShDescriptorSet != nullptr);

            computeShDescriptorSet->SetElement("InColorCubemap", colorAttachment->GetImageView());
            computeShDescriptorSet->SetElement("InNormalsCubemap", normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
            computeShDescriptorSet->SetElement("InDepthCubemap", depthAttachment ? depthAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
            computeShDescriptorSet->SetElement("InputSHTilesBuffer", shTilesBuffers[i]);

            if (i != ShNumLevels - 1)
            {
                computeShDescriptorSet->SetElement("OutputSHTilesBuffer", shTilesBuffers[i + 1]);
            }
            else
            {
                computeShDescriptorSet->SetElement("OutputSHTilesBuffer", shTilesBuffers[i]);
            }
        }

        DeferCreate(computeShDescriptorTables[i]);
    }

    for (auto& it : pipelines)
    {
        ComputePipelineRef& pipeline = it.second;

        pipeline = g_renderBackend->MakeComputePipeline(
            it.first,
            computeShDescriptorTables[0]);

        Assert(pipeline->Create());
    }

    // Bind a directional light and sky envprobe if available
    EnvProbe* skyProbe = nullptr;
    Light* directionalLight = nullptr;

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() == LT_DIRECTIONAL)
        {
            AssertDebug(RenderApi::RetrieveResourceBinding(light) != ~0u, "Light not bound!");

            directionalLight = light;

            break;
        }
    }

    if (const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
    {
        skyProbe = skyProbes.Front();
        AssertDebug(skyProbe != nullptr);
        AssertDebug(skyProbe->IsA<SkyProbe>());
    }

    const Vec2u cubemapDimensions = colorAttachment->GetImage()->GetExtent().GetXY();

    struct
    {
        Vec4u probeGridPosition;
        Vec4u cubemapDimensions;
        Vec4u levelDimensions;
        Vec4f worldPosition;
        uint32 envProbeIndex;
    } pushConstants;

    pushConstants.envProbeIndex = RenderApi::RetrieveResourceBinding(envProbe);
    pushConstants.probeGridPosition = { 0, 0, 0, 0 };
    pushConstants.cubemapDimensions = Vec4u { cubemapDimensions, 0, 0 };
    pushConstants.worldPosition = envProbeProxy->bufferData.worldPosition;

    AssertDebug(pushConstants.envProbeIndex != ~0u);

    pipelines[MODE_CLEAR].second->SetPushConstants(&pushConstants, sizeof(pushConstants));
    pipelines[MODE_BUILD_COEFFICIENTS].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

    RenderQueue* asyncRenderQueuePtr = g_renderBackend->GetAsyncCompute()->IsSupported()
        ? &g_renderBackend->GetAsyncCompute()->renderQueue
        : &frame->renderQueue;

    RenderQueue& asyncRenderQueue = *asyncRenderQueuePtr;

    asyncRenderQueue << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncRenderQueue << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncRenderQueue << BindDescriptorTable(
        computeShDescriptorTables[0],
        pipelines[MODE_CLEAR].second,
        { { "Global",
            { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncRenderQueue << BindComputePipeline(pipelines[MODE_CLEAR].second);
    asyncRenderQueue << DispatchCompute(pipelines[MODE_CLEAR].second, Vec3u { 1, 1, 1 });

    asyncRenderQueue << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncRenderQueue << BindDescriptorTable(
        computeShDescriptorTables[0],
        pipelines[MODE_BUILD_COEFFICIENTS].second,
        { { "Global",
            { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncRenderQueue << BindComputePipeline(pipelines[MODE_BUILD_COEFFICIENTS].second);
    asyncRenderQueue << DispatchCompute(pipelines[MODE_BUILD_COEFFICIENTS].second, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (ShParallelReduce)
    {
        for (uint32 i = 1; i < ShNumLevels; i++)
        {
            asyncRenderQueue << InsertBarrier(
                shTilesBuffers[i - 1],
                RS_UNORDERED_ACCESS,
                SMT_COMPUTE);

            const Vec2u prevDimensions {
                MathUtil::Max(1u, ShNumSamples.x >> (i - 1)),
                MathUtil::Max(1u, ShNumSamples.y >> (i - 1))
            };

            const Vec2u nextDimensions {
                MathUtil::Max(1u, ShNumSamples.x >> i),
                MathUtil::Max(1u, ShNumSamples.y >> i)
            };

            Assert(prevDimensions.x >= 2);
            Assert(prevDimensions.x > nextDimensions.x);
            Assert(prevDimensions.y > nextDimensions.y);

            pushConstants.levelDimensions = {
                prevDimensions.x,
                prevDimensions.y,
                nextDimensions.x,
                nextDimensions.y
            };

            pipelines[MODE_REDUCE].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

            asyncRenderQueue << BindDescriptorTable(
                computeShDescriptorTables[i - 1],
                pipelines[MODE_REDUCE].second,
                { { "Global",
                    { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                        { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
                frame->GetFrameIndex());

            asyncRenderQueue << BindComputePipeline(pipelines[MODE_REDUCE].second);
            asyncRenderQueue << DispatchCompute(pipelines[MODE_REDUCE].second, Vec3u { 1, (nextDimensions.x + 3) / 4, (nextDimensions.y + 3) / 4 });
        }
    }

    const uint32 finalizeShBufferIndex = ShParallelReduce ? ShNumLevels - 1 : 0;

    // Finalize - build into final buffer
    asyncRenderQueue << InsertBarrier(shTilesBuffers[finalizeShBufferIndex], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncRenderQueue << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    pipelines[MODE_FINALIZE].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

    asyncRenderQueue << BindDescriptorTable(
        computeShDescriptorTables[finalizeShBufferIndex],
        pipelines[MODE_FINALIZE].second,
        { { "Global",
            { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncRenderQueue << BindComputePipeline(pipelines[MODE_FINALIZE].second);
    asyncRenderQueue << DispatchCompute(pipelines[MODE_FINALIZE].second, Vec3u { 1, 1, 1 });

    asyncRenderQueue << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = frame->OnFrameEnd.Bind([envProbe = MakeStrongRef(envProbe), pipelines = std::move(pipelines), descriptorTables = std::move(computeShDescriptorTables), delegateHandle](FrameBase* frame) mutable
        {
            HYP_NAMED_SCOPE("EnvProbe::ComputeSH - Buffer readback");

            const uint32 boundIndex = RenderApi::RetrieveResourceBinding(envProbe);
            Assert(boundIndex != ~0u);

            EnvProbeShaderData readbackBuffer;

            // g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->ReadbackElement(frame->GetFrameIndex(), boundIndex, &readbackBuffer);

            // // Enqueue on game thread, not safe to write on render thread.
            // GetThreadById(g_gameThread)->GetScheduler().Enqueue([envProbe = std::move(envProbe), shData = readbackBuffer.sh]() mutable
            //     {
            //         HYP_LOG(Rendering, Debug, "EnvProbe {} SH data computed:", envProbe->Id());
            //         for (uint32 i = 0; i < 9; i++)
            //         {
            //             HYP_LOG(Rendering, Debug, "\tSH[{}] = {}", i, shData.values[i]);
            //         }

            //         envProbe->SetSphericalHarmonicsData(shData);

            //         SafeDelete(std::move(envProbe));
            //     },
            //     TaskEnqueueFlags::FIRE_AND_FORGET);

            for (auto& it : pipelines)
            {
                ShaderRef& shader = it.first;
                ComputePipelineRef& pipeline = it.second;

                SafeDelete(std::move(shader));
                SafeDelete(std::move(pipeline));
            }

            SafeDelete(std::move(descriptorTables));

            delete delegateHandle;
        });
}

#pragma endregion ReflectionProbeRenderer

} // namespace hyperion
