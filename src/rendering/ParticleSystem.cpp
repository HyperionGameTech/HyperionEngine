/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ParticleSystem.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <scene/View.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/math/MathUtil.hpp>

#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#endif

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateParticleSpawnerBuffers)
    : RenderCommand
{
    GpuBufferRef particleBuffer;
    GpuBufferRef indirectBuffer;
    GpuBufferRef noiseBuffer;
    ParticleSpawnerParams params;

    RENDER_COMMAND(CreateParticleSpawnerBuffers)(
        GpuBufferRef particleBuffer,
        GpuBufferRef indirectBuffer,
        GpuBufferRef noiseBuffer,
        const ParticleSpawnerParams& params)
        : particleBuffer(std::move(particleBuffer)),
          indirectBuffer(std::move(indirectBuffer)),
          noiseBuffer(std::move(noiseBuffer)),
          params(params)
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSpawnerBuffers)() override
    {
        SafeRelease(std::move(particleBuffer));
        SafeRelease(std::move(indirectBuffer));
        SafeRelease(std::move(noiseBuffer));
    }

    virtual RendererResult operator()() override
    {
        static constexpr uint32 seed = 0xff;

        Bitmap<1> noiseMap = SimplexNoiseGenerator(seed).CreateBitmap(128, 128, 1024.0f);

        HYP_GFX_CHECK(particleBuffer->Create());
        HYP_GFX_CHECK(indirectBuffer->Create());
        HYP_GFX_CHECK(noiseBuffer->Create());

        // copy zeroes into particle buffer
        // if we don't do this, garbage values could be in the particle buffer,
        // meaning we'd get some crazy high lifetimes
        particleBuffer->Memset(particleBuffer->Size(), 0);

        // copy bytes into noise buffer
        Array<float> unpackedFloats = noiseMap.GetUnpackedFloats();
        Assert(noiseBuffer->Size() == unpackedFloats.ByteSize());

        noiseBuffer->Copy(unpackedFloats.ByteSize(), unpackedFloats.Data());

        // don't need it anymore
        noiseMap = Bitmap<1>();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyParticleSystem)
    : RenderCommand
{
    ThreadSafeContainer<ParticleSpawner>* spawners;

    RENDER_COMMAND(DestroyParticleSystem)(
        ThreadSafeContainer<ParticleSpawner>* spawners)
        : spawners(spawners)
    {
    }

    virtual ~RENDER_COMMAND(DestroyParticleSystem)() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        if (spawners->HasUpdatesPending())
        {
            spawners->UpdateItems();
        }

        spawners->Clear();

        return result;
    }
};

struct RENDER_COMMAND(CreateParticleSystemBuffers)
    : RenderCommand
{
    GpuBufferRef stagingBuffer;
    ByteBuffer indirectDrawCommandsBuffer;

    RENDER_COMMAND(CreateParticleSystemBuffers)(GpuBufferRef stagingBuffer, ByteBuffer&& indirectDrawCommandsBuffer)
        : stagingBuffer(std::move(stagingBuffer)),
          indirectDrawCommandsBuffer(std::move(indirectDrawCommandsBuffer))
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSystemBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(stagingBuffer->Create());

        // copy zeros to buffer
        stagingBuffer->Copy(indirectDrawCommandsBuffer.Size(), indirectDrawCommandsBuffer.Data());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region ParticleSpawner

ParticleSpawner::ParticleSpawner()
    : m_params {}
{
}

ParticleSpawner::ParticleSpawner(const ParticleSpawnerParams& params)
    : m_params(params)
{
}

ParticleSpawner::~ParticleSpawner()
{
    SafeRelease(std::move(m_graphicsPipeline));
    SafeRelease(std::move(m_updateParticles));
    SafeRelease(std::move(m_particleBuffer));
    SafeRelease(std::move(m_indirectBuffer));
    SafeRelease(std::move(m_noiseBuffer));

    m_shader.Reset();
}

void ParticleSpawner::Init()
{
    InitObject(m_params.texture);

    CreateBuffers();
    CreateComputePipelines();
    CreateGraphicsPipeline();

    SetReady(true);
}

void ParticleSpawner::CreateBuffers()
{
    m_particleBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, m_params.maxParticles * sizeof(ParticleShaderData));
    m_indirectBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
    m_noiseBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(float) * 128 * 128);

    PUSH_RENDER_COMMAND(
        CreateParticleSpawnerBuffers,
        m_particleBuffer,
        m_indirectBuffer,
        m_noiseBuffer,
        m_params);
}

void ParticleSpawner::CreateGraphicsPipeline()
{
    m_shader = g_shaderManager->GetOrCreate(NAME("Particle"));
    Assert(m_shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ParticleDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("ParticlesBuffer", m_particleBuffer);
        descriptorSet->SetElement("ParticleTexture", m_params.texture ? g_renderBackend->GetTextureImageView(m_params.texture) : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
    }

    DeferCreate(descriptorTable);

#if 0
    m_graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        descriptorTable,
        { &m_framebuffer, 1 },
        RenderableAttributeSet(
            MeshAttributes {
                .vertexAttributes = staticMeshVertexAttributes },
            MaterialAttributes {
                .bucket = RB_TRANSLUCENT,
                .blendFunction = BlendFunction::Additive(),
                .cullFaces = FCM_FRONT,
                .flags = MAF_DEPTH_TEST }));
#endif

    // // @FIXME: needs to be per view!
    // m_renderGroup->AddFramebuffer(g_engineDriver->GetCurrentView()->GetGBuffer()->GetBucket(RB_TRANSLUCENT).GetFramebuffer());
}

void ParticleSpawner::CreateComputePipelines()
{
    ShaderProperties properties;
    properties.Set(NAME("HAS_PHYSICS"), m_params.hasPhysics);

    ShaderRef updateParticlesShader = g_shaderManager->GetOrCreate(NAME("UpdateParticles"), properties);
    Assert(updateParticlesShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = updateParticlesShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("UpdateParticlesDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("ParticlesBuffer", m_particleBuffer);
        descriptorSet->SetElement("IndirectDrawCommandsBuffer", m_indirectBuffer);
        descriptorSet->SetElement("NoiseBuffer", m_noiseBuffer);
    }

    DeferCreate(descriptorTable);

    m_updateParticles = g_renderBackend->MakeComputePipeline(updateParticlesShader, descriptorTable);
    DeferCreate(m_updateParticles);
}

#pragma endregion ParticleSpawner

#pragma region ParticleSystem

ParticleSystem::ParticleSystem()
    : HypObjectBase(),
      m_particleSpawners(g_renderThread),
      m_counter(0u)
{
}

ParticleSystem::~ParticleSystem()
{
    SafeRelease(std::move(m_stagingBuffer));

    m_quadMesh.Reset();

    PUSH_RENDER_COMMAND(
        DestroyParticleSystem,
        &m_particleSpawners);

    HYP_SYNC_RENDER();
}

void ParticleSystem::Init()
{
    m_quadMesh = MeshBuilder::Quad();
    InitObject(m_quadMesh);

    CreateBuffers();

    SetReady(true);
}

void ParticleSystem::CreateBuffers()
{
    ByteBuffer indirectDrawCommandsBuffer;
    g_renderBackend->PopulateIndirectDrawCommandsBuffer(
        m_quadMesh->GetVertexBuffer(),
        m_quadMesh->GetIndexBuffer(),
        0, indirectDrawCommandsBuffer);

    m_stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, indirectDrawCommandsBuffer.Size());

    PUSH_RENDER_COMMAND(CreateParticleSystemBuffers, m_stagingBuffer, std::move(indirectDrawCommandsBuffer));
}

void ParticleSystem::UpdateParticles(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);
    AssertReady();

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    if (m_particleSpawners.GetItems().Empty())
    {
        if (m_particleSpawners.HasUpdatesPending())
        {
            m_particleSpawners.UpdateItems();
        }

        return;
    }

    frame->renderQueue << InsertBarrier(m_stagingBuffer, RS_COPY_SRC);

    for (auto& spawner : m_particleSpawners)
    {
        const SizeType maxParticles = spawner->GetParams().maxParticles;

        Assert(spawner->GetIndirectBuffer()->Size() == sizeof(IndirectDrawCommand));
        Assert(spawner->GetParticleBuffer()->Size() >= sizeof(ParticleShaderData) * maxParticles);

        frame->renderQueue << InsertBarrier(
            spawner->GetIndirectBuffer(),
            RS_COPY_DST);

        // copy zeros to buffer (to reset instance count)
        frame->renderQueue << CopyBuffer(
            m_stagingBuffer,
            spawner->GetIndirectBuffer(),
            sizeof(IndirectDrawCommand));

        frame->renderQueue << InsertBarrier(
            spawner->GetIndirectBuffer(),
            RS_INDIRECT_ARG);

        // @FIXME: frustum needs to be added to buffer data
        // if (activeCamera != nullptr && !(*activeCamera)->GetBufferData().frustum.ContainsBoundingSphere(spawner->GetBoundingSphere())) {
        //     continue;
        // }

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

        pushConstants.origin = Vec4f(spawner->GetParams().origin, spawner->GetParams().startSize);
        pushConstants.spawnRadius = spawner->GetParams().radius;
        pushConstants.randomness = spawner->GetParams().randomness;
        pushConstants.avgLifespan = spawner->GetParams().lifespan;
        pushConstants.maxParticles = uint32(maxParticles);
        pushConstants.maxParticlesSqrt = MathUtil::Sqrt(float(maxParticles));
        pushConstants.deltaTime = 0.016f; // TODO! Delta time for particles. we currentl don't have delta time for render thread.
        pushConstants.globalCounter = m_counter;

        spawner->GetComputePipeline()->SetPushConstants(&pushConstants, sizeof(pushConstants));

        frame->renderQueue << BindComputePipeline(spawner->GetComputePipeline());

        frame->renderQueue << BindDescriptorTable(
            spawner->GetComputePipeline()->GetDescriptorTable(),
            spawner->GetComputePipeline(),
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frame->GetFrameIndex());

        const uint32 viewDescriptorSetIndex = spawner->GetComputePipeline()->GetDescriptorTable()->GetDescriptorSetIndex("View");

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);

            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
                spawner->GetComputePipeline(),
                {},
                viewDescriptorSetIndex);
        }

        frame->renderQueue << DispatchCompute(
            spawner->GetComputePipeline(),
            Vec3u { uint32((maxParticles + 255) / 256), 1, 1 });

        frame->renderQueue << InsertBarrier(
            spawner->GetIndirectBuffer(),
            RS_INDIRECT_ARG);
    }

    ++m_counter;

    // update render-side container after render,
    // so that all objects get initialized before the next render call.
    if (m_particleSpawners.HasUpdatesPending())
    {
        m_particleSpawners.UpdateItems();
    }
}

void ParticleSystem::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);
    AssertReady();

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    for (const Handle<ParticleSpawner>& particleSpawner : m_particleSpawners.GetItems())
    {
        const GraphicsPipelineRef& graphicsPipeline = particleSpawner->GetGraphicsPipeline();

        frame->renderQueue << BindGraphicsPipeline(graphicsPipeline);

        frame->renderQueue << BindDescriptorTable(
            graphicsPipeline->GetDescriptorTable(),
            graphicsPipeline,
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frameIndex);

        const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);

            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frameIndex],
                graphicsPipeline,
                {},
                viewDescriptorSetIndex);
        }

        frame->renderQueue << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        frame->renderQueue << BindIndexBuffer(m_quadMesh->GetIndexBuffer());
        frame->renderQueue << DrawIndexedIndirect(particleSpawner->GetIndirectBuffer(), 0);
    }
}

#pragma endregion ParticleSystem

} // namespace hyperion
