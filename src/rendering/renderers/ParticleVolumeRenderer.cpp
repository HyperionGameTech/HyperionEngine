/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/renderers/ParticleVolumeRenderer.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <particles/ParticleVolume.hpp>

#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <core/math/MathUtil.hpp>

#include <engine/EngineGlobals.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#endif

namespace hyperion {

ParticleVolumeRenderer::ParticleVolumeRenderer() = default;
ParticleVolumeRenderer::~ParticleVolumeRenderer() = default;

void ParticleVolumeRenderer::Initialize()
{
    HYP_SCOPE;
}

void ParticleVolumeRenderer::Shutdown()
{
    HYP_SCOPE;

    for (auto& it : m_volumeStates)
    {
        SafeDelete(std::move(it.second.updatePipeline));
        SafeDelete(std::move(it.second.graphicsPipeline));
        SafeDelete(std::move(it.second.particleBuffer));
        SafeDelete(std::move(it.second.indirectBuffer));
        SafeDelete(std::move(it.second.noiseBuffer));
    }

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

static void CreateNoiseBuffer(const GpuBufferRef& noiseBuffer)
{
    static constexpr uint32 Seed = 0xff;

    Bitmap_R8 noiseMap = SimplexNoiseGenerator(Seed).CreateBitmap(128, 128, 1024.0f);

    Array<float> unpacked = noiseMap.GetUnpackedFloats();
    Assert(noiseBuffer->Size() == unpacked.ByteSize());
    DeferCreate(noiseBuffer);

    noiseBuffer->Copy(unpacked.ByteSize(), unpacked.Data());
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
    state.particleBuffer->SetRequireCpuAccessible(true);
    DeferCreate(state.particleBuffer);

    state.indirectBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
    state.indirectBuffer->SetRequireCpuAccessible(true);
    DeferCreate(state.indirectBuffer);

    state.noiseBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(float) * 128 * 128);
    state.noiseBuffer->SetRequireCpuAccessible(true);
    CreateNoiseBuffer(state.noiseBuffer);

    // compute pipeline
    ShaderProperties properties;

    // hasPhysics comes from entity params; if needed, read from WeakHandle
    state.hasPhysics = false; // default, renderer can override when binding push constants
    properties.Set(NAME("HAS_PHYSICS"), state.hasPhysics);

    state.updateShader = g_shaderManager->GetOrCreate(NAME("UpdateParticles"), properties);
    Assert(state.updateShader.IsValid());

    state.computeDescriptorTable = g_renderBackend->MakeDescriptorTable(state.updateShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; ++frameIndex)
    {
        const DescriptorSetRef& descriptorSet = state.computeDescriptorTable->GetDescriptorSet("UpdateParticlesDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("ParticlesBuffer", state.particleBuffer);
        descriptorSet->SetElement("IndirectDrawCommandsBuffer", state.indirectBuffer);
        descriptorSet->SetElement("NoiseBuffer", state.noiseBuffer);
    }

    DeferCreate(state.computeDescriptorTable);

    state.updatePipeline = g_renderBackend->MakeComputePipeline(state.updateShader, state.computeDescriptorTable);
    DeferCreate(state.updatePipeline);

    // graphics pipeline
    state.particleShader = g_shaderManager->GetOrCreate(NAME("Particle"));
    Assert(state.particleShader.IsValid());

    state.graphicsDescriptorTable = g_renderBackend->MakeDescriptorTable(
        state.particleShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; ++frameIndex)
    {
        const DescriptorSetRef& descriptorSet = state.graphicsDescriptorTable->GetDescriptorSet("ParticleDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("ParticlesBuffer", state.particleBuffer);
        descriptorSet->SetElement("ParticleTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
    }

    DeferCreate(state.graphicsDescriptorTable);

    return state;
}

void ParticleVolumeRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    EnsureStaging();

    View* view = renderSetup.view;
    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);

    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    auto& particleVolumes = rpl.GetParticleVolumes();

    if (particleVolumes.NumCurrent() == 0)
    {
        return;
    }

    // Reset zero staging buffer state
    frame->renderQueue << InsertBarrier(m_staging.zeroIndirectArgs, RS_COPY_SRC);

    const uint32 frameIndex = frame->GetFrameIndex();

    for (auto it = particleVolumes.Begin(); it != particleVolumes.End(); ++it)
    {
        RenderProxyParticleVolume* pProxy = static_cast<RenderProxyParticleVolume*>(RenderApi::GetRenderProxy(*it));

        if (!pProxy)
        {
            continue;
        }

        VolumeState& state = EnsureVolumeState(pProxy);

        // ensure particle texture bound
        if (pProxy->particleTexture)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                const DescriptorSetRef& descriptorSet = state.graphicsDescriptorTable->GetDescriptorSet("ParticleDescriptorSet", frameIndex);
                AssertDebug(descriptorSet != nullptr);

                descriptorSet->SetElement("ParticleTexture", g_renderBackend->GetTextureImageView(MakeStrongRef(pProxy->particleTexture)));
            }
        }

        // zero indirect arguments (instance count)
        Assert(state.indirectBuffer->Size() == sizeof(IndirectDrawCommand));

        frame->renderQueue << InsertBarrier(state.indirectBuffer, RS_COPY_DST);
        frame->renderQueue << CopyBuffer(m_staging.zeroIndirectArgs, state.indirectBuffer, sizeof(IndirectDrawCommand));
        frame->renderQueue << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);

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

        pushConstants.origin = pProxy->bufferData.originStartSize;
        pushConstants.spawnRadius = pProxy->bufferData.spawnRadius;
        pushConstants.randomness = pProxy->bufferData.randomness;
        pushConstants.avgLifespan = pProxy->bufferData.avgLifespan;
        pushConstants.maxParticles = pProxy->bufferData.maxParticles;
        pushConstants.maxParticlesSqrt = pProxy->bufferData.maxParticlesSqrt;
        pushConstants.deltaTime = 0.016f; // TODO: real render delta
        pushConstants.globalCounter = m_counter;

        state.updatePipeline->SetPushConstants(&pushConstants, sizeof(pushConstants));

        frame->renderQueue << BindComputePipeline(state.updatePipeline);

        frame->renderQueue << BindDescriptorTable(
            state.computeDescriptorTable,
            state.updatePipeline,
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(view->GetCamera()) } } } },
            frameIndex);

        uint32 viewDescriptorSetIndex = state.computeDescriptorTable->GetDescriptorSetIndex("View");
        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);
            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frameIndex],
                state.updatePipeline,
                {},
                viewDescriptorSetIndex);
        }

        const SizeType maxParticles = pProxy->bufferData.maxParticles;
        frame->renderQueue << DispatchCompute(state.updatePipeline, Vec3u { uint32((maxParticles + 255) / 256), 1, 1 });

        frame->renderQueue << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);

        if (!state.graphicsPipeline)
        {
            // we rely on the pipeline cache via descriptor table + shader; framebuffer comes from view
            // Using pipeline cache would be preferable; for now, bind directly each frame
            
            state.graphicsPipeline = g_renderBackend->MakeGraphicsPipeline(
                state.particleShader,
                state.graphicsDescriptorTable);
            
            DeferCreate(state.graphicsPipeline);
        }

        // draw
        frame->renderQueue << BindGraphicsPipeline(state.graphicsPipeline, view->GetViewport());

        frame->renderQueue << BindDescriptorTable(
            state.graphicsDescriptorTable,
            state.graphicsPipeline,
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(view->GetCamera()) } } } },
            frameIndex);

        viewDescriptorSetIndex = state.graphicsDescriptorTable->GetDescriptorSetIndex("View");

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);

            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frameIndex],
                state.graphicsPipeline,
                {},
                viewDescriptorSetIndex);
        }

        frame->renderQueue << BindVertexBuffer(m_staging.quadMesh->GetVertexBuffer());
        frame->renderQueue << BindIndexBuffer(m_staging.quadMesh->GetIndexBuffer());
        frame->renderQueue << DrawIndexedIndirect(state.indirectBuffer, 0);
    }

    ++m_counter;
}

} // namespace hyperion
