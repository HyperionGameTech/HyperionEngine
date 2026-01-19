/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/ParticleVolumeRenderer.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/ParticleVolume.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <core/math/MathUtil.hpp>

#if HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#endif

namespace Hyperion {

// How many frames until we release resources for unused volumes?
static constexpr uint32 DiscardFrames = 60;

ParticleVolumeRenderer::VolumeState::~VolumeState()
{
    SafeDelete(std::move(updatePipeline));
    SafeDelete(std::move(particleBuffer));
    SafeDelete(std::move(indirectBuffer));
    SafeDelete(std::move(noiseMap));
    SafeDelete(std::move(computeDescriptorTable));
    SafeDelete(std::move(graphicsDescriptorTable));
    SafeDelete(std::move(updateShader));
    SafeDelete(std::move(particleShader));
    SafeDelete(std::move(updatePipeline));
}

ParticleVolumeRenderer::ParticleVolumeRenderer() = default;
ParticleVolumeRenderer::~ParticleVolumeRenderer() = default;

void ParticleVolumeRenderer::Initialize()
{
    HYP_SCOPE;
}

void ParticleVolumeRenderer::Shutdown()
{
    HYP_SCOPE;

    if (m_staging.quadMesh.IsValid())
    {
        SafeDelete(std::move(m_staging.quadMesh));
    }

    SafeDelete(std::move(m_staging.zeroIndirectArgs));
}

Handle<PassData> ParticleVolumeRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    Handle<PassData> pd = CreateObject<PassData>();
    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

    return pd;
}

void ParticleVolumeRenderer::EnsureStaging()
{
    if (!m_staging.quadMesh.IsValid())
    {
        m_staging.quadMesh = MeshBuilder::Quad();
        m_staging.quadMesh->SetFlags(MF_VIEW_INDEPENDENT);
        InitObject(m_staging.quadMesh);
    }

    if (!m_staging.zeroIndirectArgs)
    {
        TByteBuffer<RenderAllocator> indirectDrawCommandsBuffer;
        g_renderBackend->PopulateIndirectDrawCommandsBuffer(
            m_staging.quadMesh->GetVertexBuffer(),
            m_staging.quadMesh->GetIndexBuffer(),
            0, indirectDrawCommandsBuffer);

        m_staging.zeroIndirectArgs = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, indirectDrawCommandsBuffer.Size());
        DeferCreate(m_staging.zeroIndirectArgs);

        m_staging.zeroIndirectArgs->Copy(indirectDrawCommandsBuffer.Size(), indirectDrawCommandsBuffer.Data());
    }
}

static void CreateNoiseMap(Handle<Texture>& tex)
{
    static constexpr uint32 Seed = 0xff;

    TextureDesc textureDesc {};
    textureDesc.extent = Vec3u { 128, 128, 1 };
    textureDesc.type = TT_TEX2D;
    textureDesc.format = TF_R8;
    textureDesc.filterModeMin = TFM_LINEAR;
    textureDesc.filterModeMag = TFM_LINEAR;

    TextureData textureData;

    Bitmap_R8 noiseMap = SimplexNoiseGenerator(Seed).CreateBitmap(128, 128, 1024.0f);
    textureData.imageData = ByteBuffer(noiseMap.ToByteView());

    if (tex)
    {
        SafeDelete(std::move(tex));
    }

    tex = CreateObject<Texture>(textureDesc, textureData);
    InitObject(tex);
}

ParticleVolumeRenderer::VolumeState& ParticleVolumeRenderer::EnsureVolumeState(RenderProxyParticleVolume* proxy)
{
    auto it = m_volumeStates.Find(proxy->particleVolume.Id());

    if (it != m_volumeStates.End())
    {
        return it->second;
    }

    VolumeState& state = m_volumeStates.Emplace(proxy->particleVolume.Id()).first->second;

    state.maxParticles = proxy->bufferData.maxParticles;

    state.particleBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, state.maxParticles * sizeof(ParticleShaderData));
    DeferCreate(state.particleBuffer);

    state.indirectBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
    DeferCreate(state.indirectBuffer);

    CreateNoiseMap(state.noiseMap);

    // compute pipeline
    ShaderProperties properties;
    state.hasPhysics = proxy->particleVolume.GetUnsafe()->GetParams().hasPhysics;
    properties.Set(NAME("HAS_PHYSICS"), state.hasPhysics);
    properties.Set(ShaderProperty(NAME("MAX_PARTICLES"), int(state.maxParticles)));

    state.updateShader = g_shaderManager->GetOrCreate(NAME("UpdateParticles"), properties);
    Assert(state.updateShader.IsValid());

    state.computeDescriptorTable = g_renderBackend->MakeDescriptorTable(state.updateShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; ++frameIndex)
    {
        const DescriptorSetRef& descriptorSet = state.computeDescriptorTable->GetDescriptorSet("UpdateParticlesDescriptorSet"_sh, frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("ParticlesBuffer"_sh, state.particleBuffer);
        descriptorSet->SetElement("IndirectDrawCommandsBuffer"_sh, state.indirectBuffer);
        descriptorSet->SetElement("NoiseMap"_sh, g_renderInterface->textureViewCache->GetOrCreate(state.noiseMap));
    }

    DeferCreate(state.computeDescriptorTable);

    state.updatePipeline = g_renderBackend->MakeComputePipeline(state.updateShader, state.computeDescriptorTable);
    DeferCreate(state.updatePipeline);

    // graphics pipeline
    properties = ShaderProperties();
    properties.Set(ShaderProperty(NAME("MAX_PARTICLES"), int(state.maxParticles)));

    state.particleShader = g_shaderManager->GetOrCreate(NAME("Particle"), properties);
    Assert(state.particleShader.IsValid());

    state.graphicsDescriptorTable = g_renderBackend->MakeDescriptorTable(state.particleShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; ++frameIndex)
    {
        const DescriptorSetRef& descriptorSet = state.graphicsDescriptorTable->GetDescriptorSet("ParticleDescriptorSet"_sh, frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("ParticlesBuffer"_sh, state.particleBuffer);
        descriptorSet->SetElement("ParticleTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    DeferCreate(state.graphicsDescriptorTable);

    // set default particle graphics attributes (translucent)
    MaterialAttributes materialAttributes {};
    materialAttributes.shaderDefinition = ShaderDefinition { NAME("Particle"), properties };
    materialAttributes.bucket = RB_TRANSLUCENT;
    materialAttributes.blendFunction = BlendFunction::AlphaBlending();
    materialAttributes.cullFaces = FCM_FRONT;
    materialAttributes.flags = MAF_DEPTH_TEST; // depth test on, depth write off by default

    MeshAttributes meshAttributes {};
    meshAttributes.vertexAttributes = VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0;
    meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshAttributes.topology = TOP_TRIANGLES;
    state.renderableAttributes = RenderableAttributeSet(meshAttributes, materialAttributes);

    return state;
}

void ParticleVolumeRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    HYP_NAMED_SCOPE("Render particle volume");

    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.volume);

    ParticleVolume* particleVolume = ObjCast<ParticleVolume>(renderSetup.volume);
    AssertDebug(particleVolume != nullptr);

    EnsureStaging();

    View* view = renderSetup.view;
    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);

    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // Reset zero staging buffer state
    frame->preRenderQueue << InsertBarrier(m_staging.zeroIndirectArgs, RS_COPY_SRC);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyParticleVolume* proxy = static_cast<RenderProxyParticleVolume*>(RenderApi::GetRenderProxy(particleVolume));
    AssertDebug(proxy != nullptr);

    VolumeState& state = EnsureVolumeState(proxy);

    // ensure particle texture bound
    if (proxy->particleTexture)
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = state.graphicsDescriptorTable->GetDescriptorSet("ParticleDescriptorSet"_sh, frameIndex);
            AssertDebug(descriptorSet != nullptr);

            descriptorSet->SetElement("ParticleTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(proxy->particleTexture));
        }
    }

    // zero indirect arguments (instance count)
    Assert(state.indirectBuffer->Size() == sizeof(IndirectDrawCommand));

    { // zero out indirect buffer (ahead of frame compute + rendering)
        RenderQueue& rq = frame->preRenderQueue;

        rq << InsertBarrier(state.indirectBuffer, RS_COPY_DST);
        rq << CopyBuffer(m_staging.zeroIndirectArgs, state.indirectBuffer, sizeof(IndirectDrawCommand));
        rq << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);
    }

    // bind and dispatch compute
    struct
    {
        Vec4f origin;
        float spawnRadius;
        float randomness;
        float avgLifespan;
        uint32 maxParticles;
        float maxParticlesSqrt;
        float deltaTime;
        uint32 globalCounter;
    } pushConstants;

    pushConstants.origin = proxy->bufferData.originStartSize;
    pushConstants.spawnRadius = proxy->bufferData.spawnRadius;
    pushConstants.randomness = proxy->bufferData.randomness;
    pushConstants.avgLifespan = proxy->bufferData.avgLifespan;
    pushConstants.maxParticles = proxy->bufferData.maxParticles;
    pushConstants.maxParticlesSqrt = proxy->bufferData.maxParticlesSqrt;
    pushConstants.deltaTime = 0.016f; // TODO: real render delta
    pushConstants.globalCounter = m_counter;

    state.updatePipeline->SetPushConstants(&pushConstants, sizeof(pushConstants));

    { // update gpu particles pass (compute, done before frame is rendered)
        RenderQueue& rq = frame->preRenderQueue;

        rq << BindComputePipeline(state.updatePipeline);

        rq << BindDescriptorTable(
            state.computeDescriptorTable,
            state.updatePipeline,
            { { "Global"_sh, { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(view->GetCamera()) } } } },
            frameIndex);

        const uint32 viewDescriptorSetIndex = state.computeDescriptorTable->GetDescriptorSetIndex("View"_sh);

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);
            rq << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frameIndex],
                state.updatePipeline,
                {},
                viewDescriptorSetIndex);
        }

        const SizeType maxParticles = proxy->bufferData.maxParticles;
        rq << DispatchCompute(state.updatePipeline, Vec3u { uint32((maxParticles + 255) / 256), 1, 1 });

        rq << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);
    }

    state.fc = RenderApi::GetFrameCounter();

    // this is rendered from translucent pass in DeferredRenderer
    const FramebufferRef& framebuffer = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    const RenderTargetDesc& renderTargetDesc = framebuffer->GetRenderTargetDesc();

    if (!state.graphicsPipelineHandle.IsAlive() || (*state.graphicsPipelineHandle)->GetRenderTargetDesc() != renderTargetDesc)
    {
        g_renderInterface->graphicsPipelineCache->GetOrCreate(
            state.renderableAttributes,
            renderTargetDesc,
            state.graphicsPipelineHandle);
    }

    { // draw particles pass
        RenderQueue& rq = frame->renderQueue;

        rq << BindGraphicsPipeline(*state.graphicsPipelineHandle, view->GetViewport());

        rq << BindDescriptorTable(
            state.graphicsDescriptorTable,
            *state.graphicsPipelineHandle,
            { { "Global"_sh, { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(view->GetCamera()) } } } },
            frameIndex);

        const uint32 viewDescriptorSetIndex = state.graphicsDescriptorTable->GetDescriptorSetIndex("View"_sh);

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);

            rq << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frameIndex],
                *state.graphicsPipelineHandle,
                {},
                viewDescriptorSetIndex);
        }

        rq << BindVertexBuffer(m_staging.quadMesh->GetVertexBuffer());
        rq << BindIndexBuffer(m_staging.quadMesh->GetIndexBuffer());
        rq << DrawIndexedIndirect(state.indirectBuffer, 0);
    }
}

int ParticleVolumeRenderer::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 currFrame = RenderApi::GetFrameCounter();

    int numCycles = 0;

    for (auto it = m_volumeStates.Begin(); it != m_volumeStates.End() && numCycles < maxIter;)
    {
        VolumeState& state = it->second;

        const int64 frameDiff = int64(currFrame) - int64(state.fc);

        if (frameDiff >= DiscardFrames)
        {
            it = m_volumeStates.Erase(it);

            ++numCycles;

            continue;
        }

        ++it;
    }

    return numCycles;
}

} // namespace Hyperion
