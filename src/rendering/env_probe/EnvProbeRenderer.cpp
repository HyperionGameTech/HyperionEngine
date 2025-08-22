/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/Deferred.hpp>
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

namespace hyperion {

static constexpr Vec2u shNumSamples = { 16, 16 };
static constexpr Vec2u shNumTiles = { 16, 16 };
static constexpr uint32 shNumLevels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(shNumSamples.Max()) + 1));
static constexpr bool shParallelReduce = false;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Matrix4, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemapDirections[i].first,
            Texture::cubemapDirections[i].second);
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
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.envProbe != nullptr);

    EnvProbe* envProbe = renderSetup.envProbe;
    AssertDebug(envProbe != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = envProbe->GetView().Get();
    rs.passData = FetchViewPassData(rs.view);

    RenderProbe(frame, rs, envProbe);
}

Handle<PassData> EnvProbeRenderer::CreateViewPassData(View* view, PassDataExt& ext)
{
    Handle<EnvProbePassData> pd = CreateObject<EnvProbePassData>();
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

    CreateShader();
}

void ReflectionProbeRenderer::Shutdown()
{
    HYP_SCOPE;

    EnvProbeRenderer::Shutdown();

    SafeDelete(std::move(m_shader));
}

void ReflectionProbeRenderer::CreateShader()
{
    HYP_SCOPE;

    AssertDebug(!m_shader.IsValid());

    m_shader = g_shaderManager->GetOrCreate(
        NAME("RenderToCubemap"),
        ShaderProperties(staticMeshVertexAttributes, { NAME("WRITE_NORMALS"), NAME("WRITE_MOMENTS") }));

    Assert(m_shader.IsValid());
}

void ReflectionProbeRenderer::RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    EnvProbePassData* pd = ObjCast<EnvProbePassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // special checks for Sky + caching result based on light position + intensity
    if (envProbe->IsA<SkyProbe>())
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

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi_GetRenderProxy(renderSetup.light));
        AssertDebug(lightProxy != nullptr);
        AssertDebug(RenderApi_RetrieveResourceBinding(renderSetup.light) != ~0u);

        if (lightProxy->bufferData.positionIntensity == pd->cachedLightDirIntensity
            && !rpl.GetMeshEntities().GetDiff().NeedsUpdate())
        {
            // no need to render it just yet if values have not changed -- return early
            return;
        }

        // cache it to save on rendering later
        pd->cachedLightDirIntensity = lightProxy->bufferData.positionIntensity;
    }

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    if (!rpl.GetMeshEntities().GetDiff().NeedsUpdate()
        && !rpl.GetLights().GetDiff().NeedsUpdate()
        && pd->cachedProbeOrigin == envProbeProxy->bufferData.worldPosition.GetXYZ())
    {
        return;
    }

    pd->cachedProbeOrigin = envProbeProxy->bufferData.worldPosition.GetXYZ();

    RenderCollector& renderCollector = RenderApi_GetRenderCollector(view);

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

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
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

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Set(NAME("LIGHTING"));
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

        uniforms.lightIndices[numBoundLights++] = RenderApi_RetrieveResourceBinding(light);
    }

    uniforms.numBoundLights = numBoundLights;
    
    HYP_LOG_TEMP("Num bound lights for env probe : {}", uniforms.numBoundLights);

    GpuBufferRef uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
    HYP_GFX_ASSERT(uniformBuffer->Create());
    uniformBuffer->Copy(sizeof(uniforms), &uniforms);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* momentsAttachment = framebuffer->GetAttachment(2);

    Assert(colorAttachment != nullptr);
    Assert(normalsAttachment != nullptr);
    Assert(momentsAttachment != nullptr);

    const DescriptorTableDeclaration& descriptorTableDecl = convolveProbeShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
    descriptorTable->SetDebugName(NAME_FMT("ConvolveProbeDescriptorTable_{}", envProbe->Id().Value()));

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ConvolveProbeDescriptorSet", frameIndex);
        Assert(descriptorSet.IsValid());

        descriptorSet->SetElement("UniformBuffer", uniformBuffer);
        descriptorSet->SetElement("ColorTexture", colorAttachment->GetImageView());
        descriptorSet->SetElement("NormalsTexture", normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement("MomentsTexture", momentsAttachment ? momentsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement("SamplerLinear", g_renderGlobalState->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement("SamplerNearest", g_renderGlobalState->placeholderData->GetSamplerNearest());
        descriptorSet->SetElement("OutImage", g_renderBackend->GetTextureImageView(prefilteredEnvMap));
    }

    HYP_GFX_ASSERT(descriptorTable->Create());

    ComputePipelineRef convolveProbeComputePipeline = g_renderBackend->MakeComputePipeline(convolveProbeShader, descriptorTable);
    HYP_GFX_ASSERT(convolveProbeComputePipeline->Create());

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

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(envProbe));
    Assert(envProbeProxy != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const ViewOutputTarget& outputTarget = envProbe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    Assert(colorAttachment != nullptr);

    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* depthAttachment = framebuffer->GetAttachment(2);

    Array<GpuBufferRef> shTilesBuffers;
    shTilesBuffers.Resize(shNumLevels);

    Array<DescriptorTableRef> shTilesDescriptorTables;
    shTilesDescriptorTables.Resize(shNumLevels);

    for (uint32 i = 0; i < shNumLevels; i++)
    {
        const SizeType size = sizeof(SHTile) * (shNumTiles.x >> i) * (shNumTiles.y >> i);

        shTilesBuffers[i] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, size);
        HYP_GFX_ASSERT(shTilesBuffers[i]->Create());
    }

    ShaderProperties shaderProperties;

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Set(NAME("LIGHTING"));
    }

    HashMap<Name, Pair<ShaderRef, ComputePipelineRef>> pipelines = {
        { NAME("Clear"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { NAME("MODE_CLEAR") } })), ComputePipelineRef() } },
        { NAME("BuildCoeffs"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { NAME("MODE_BUILD_COEFFICIENTS") } })), ComputePipelineRef() } },
        { NAME("Reduce"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { NAME("MODE_REDUCE") } })), ComputePipelineRef() } },
        { NAME("Finalize"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { NAME("MODE_FINALIZE") } })), ComputePipelineRef() } }
    };

    ShaderRef firstShader;

    for (auto& it : pipelines)
    {
        Assert(it.second.first.IsValid());

        if (!firstShader)
        {
            firstShader = it.second.first;
        }
    }

    const DescriptorTableDeclaration& descriptorTableDecl = firstShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    Array<DescriptorTableRef> computeShDescriptorTables;
    computeShDescriptorTables.Resize(shNumLevels);

    for (uint32 i = 0; i < shNumLevels; i++)
    {
        computeShDescriptorTables[i] = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& computeShDescriptorSet = computeShDescriptorTables[i]->GetDescriptorSet("ComputeSHDescriptorSet", frameIndex);
            Assert(computeShDescriptorSet != nullptr);

            computeShDescriptorSet->SetElement("InColorCubemap", colorAttachment->GetImageView());
            computeShDescriptorSet->SetElement("InNormalsCubemap", normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
            computeShDescriptorSet->SetElement("InDepthCubemap", depthAttachment ? depthAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
            computeShDescriptorSet->SetElement("InputSHTilesBuffer", shTilesBuffers[i]);

            if (i != shNumLevels - 1)
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
        ComputePipelineRef& pipeline = it.second.second;

        pipeline = g_renderBackend->MakeComputePipeline(
            it.second.first,
            computeShDescriptorTables[0]);

        HYP_GFX_ASSERT(pipeline->Create());
    }

    // Bind a directional light and sky envprobe if available
    EnvProbe* skyProbe = nullptr;
    Light* directionalLight = nullptr;

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() == LT_DIRECTIONAL)
        {
            AssertDebug(RenderApi_RetrieveResourceBinding(light) != ~0u, "Light not bound!");

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

    pushConstants.envProbeIndex = RenderApi_RetrieveResourceBinding(envProbe);
    pushConstants.probeGridPosition = { 0, 0, 0, 0 };
    pushConstants.cubemapDimensions = Vec4u { cubemapDimensions, 0, 0 };
    pushConstants.worldPosition = envProbeProxy->bufferData.worldPosition;

    AssertDebug(pushConstants.envProbeIndex != ~0u);

    pipelines[NAME("Clear")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));
    pipelines[NAME("BuildCoeffs")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

    RenderQueue* asyncRenderQueuePtr = g_renderBackend->GetAsyncCompute()->IsSupported()
        ? &g_renderBackend->GetAsyncCompute()->renderQueue
        : &frame->renderQueue;

    RenderQueue& asyncRenderQueue = *asyncRenderQueuePtr;

    asyncRenderQueue << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncRenderQueue << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncRenderQueue << BindDescriptorTable(
        computeShDescriptorTables[0],
        pipelines[NAME("Clear")].second,
        { { "Global",
            { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncRenderQueue << BindComputePipeline(pipelines[NAME("Clear")].second);
    asyncRenderQueue << DispatchCompute(pipelines[NAME("Clear")].second, Vec3u { 1, 1, 1 });

    asyncRenderQueue << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncRenderQueue << BindDescriptorTable(
        computeShDescriptorTables[0],
        pipelines[NAME("BuildCoeffs")].second,
        { { "Global",
            { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncRenderQueue << BindComputePipeline(pipelines[NAME("BuildCoeffs")].second);
    asyncRenderQueue << DispatchCompute(pipelines[NAME("BuildCoeffs")].second, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (shParallelReduce)
    {
        for (uint32 i = 1; i < shNumLevels; i++)
        {
            asyncRenderQueue << InsertBarrier(
                shTilesBuffers[i - 1],
                RS_UNORDERED_ACCESS,
                SMT_COMPUTE);

            const Vec2u prevDimensions {
                MathUtil::Max(1u, shNumSamples.x >> (i - 1)),
                MathUtil::Max(1u, shNumSamples.y >> (i - 1))
            };

            const Vec2u nextDimensions {
                MathUtil::Max(1u, shNumSamples.x >> i),
                MathUtil::Max(1u, shNumSamples.y >> i)
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

            pipelines[NAME("Reduce")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

            asyncRenderQueue << BindDescriptorTable(
                computeShDescriptorTables[i - 1],
                pipelines[NAME("Reduce")].second,
                { { "Global",
                    { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                        { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
                frame->GetFrameIndex());

            asyncRenderQueue << BindComputePipeline(pipelines[NAME("Reduce")].second);
            asyncRenderQueue << DispatchCompute(pipelines[NAME("Reduce")].second, Vec3u { 1, (nextDimensions.x + 3) / 4, (nextDimensions.y + 3) / 4 });
        }
    }

    const uint32 finalizeShBufferIndex = shParallelReduce ? shNumLevels - 1 : 0;

    // Finalize - build into final buffer
    asyncRenderQueue << InsertBarrier(shTilesBuffers[finalizeShBufferIndex], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncRenderQueue << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    pipelines[NAME("Finalize")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

    asyncRenderQueue << BindDescriptorTable(
        computeShDescriptorTables[finalizeShBufferIndex],
        pipelines[NAME("Finalize")].second,
        { { "Global",
            { { "CurrentLight", ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                { "CurrentEnvProbe", ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncRenderQueue << BindComputePipeline(pipelines[NAME("Finalize")].second);
    asyncRenderQueue << DispatchCompute(pipelines[NAME("Finalize")].second, Vec3u { 1, 1, 1 });

    asyncRenderQueue << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    //DelegateHandler* delegateHandle = new DelegateHandler();
    //*delegateHandle = frame->OnFrameEnd.Bind([envProbe = MakeStrongRef(envProbe), pipelines = std::move(pipelines), descriptorTables = std::move(computeShDescriptorTables), delegateHandle](FrameBase* frame) mutable
    //    {
    //        HYP_NAMED_SCOPE("EnvProbe::ComputeSH - Buffer readback");

    //        const uint32 boundIndex = RenderApi_RetrieveResourceBinding(envProbe);
    //        Assert(boundIndex != ~0u);

    //        EnvProbeShaderData readbackBuffer;

    //        g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->ReadbackElement(frame->GetFrameIndex(), boundIndex, &readbackBuffer);

    //        // Enqueue on game thread, not safe to write on render thread.
    //        Threads::GetThread(g_gameThread)->GetScheduler().Enqueue([envProbe = std::move(envProbe), shData = readbackBuffer.sh]() mutable
    //            {
    //                HYP_LOG(Rendering, Debug, "EnvProbe {} SH data computed:", envProbe->Id());
    //                for (uint32 i = 0; i < 9; i++)
    //                {
    //                    HYP_LOG(Rendering, Debug, "\tSH[{}] = {}", i, shData.values[i]);
    //                }

    //                envProbe->SetSphericalHarmonicsData(shData);

    //                SafeDelete(std::move(envProbe));
    //            },
    //            TaskEnqueueFlags::FIRE_AND_FORGET);

    //        for (auto& it : pipelines)
    //        {
    //            ShaderRef& shader = it.second.first;
    //            ComputePipelineRef& pipeline = it.second.second;

    //            SafeDelete(std::move(shader));
    //            SafeDelete(std::move(pipeline));
    //        }

    //        SafeDelete(std::move(descriptorTables));

    //        delete delegateHandle;
    //    });
}

#pragma endregion ReflectionProbeRenderer

} // namespace hyperion
