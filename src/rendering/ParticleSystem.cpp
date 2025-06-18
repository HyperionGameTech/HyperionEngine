/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ParticleSystem.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/Mesh.hpp>
#include <scene/Texture.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/math/MathUtil.hpp>

#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::CommandBufferType;
using renderer::GPUBufferType;
using renderer::IndirectDrawCommand;

#pragma region Render commands

struct RENDER_COMMAND(CreateParticleSpawnerBuffers)
    : renderer::RenderCommand
{
    GPUBufferRef particle_buffer;
    GPUBufferRef indirect_buffer;
    GPUBufferRef noise_buffer;
    ParticleSpawnerParams params;

    RENDER_COMMAND(CreateParticleSpawnerBuffers)(
        GPUBufferRef particle_buffer,
        GPUBufferRef indirect_buffer,
        GPUBufferRef noise_buffer,
        const ParticleSpawnerParams& params)
        : particle_buffer(std::move(particle_buffer)),
          indirect_buffer(std::move(indirect_buffer)),
          noise_buffer(std::move(noise_buffer)),
          params(params)
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSpawnerBuffers)() override
    {
        SafeRelease(std::move(particle_buffer));
        SafeRelease(std::move(indirect_buffer));
        SafeRelease(std::move(noise_buffer));
    }

    virtual RendererResult operator()() override
    {
        static constexpr uint32 seed = 0xff;

        Bitmap<1> noise_map = SimplexNoiseGenerator(seed).CreateBitmap(128, 128, 1024.0f);

        HYPERION_BUBBLE_ERRORS(particle_buffer->Create());
        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create());
        HYPERION_BUBBLE_ERRORS(noise_buffer->Create());

        // copy zeroes into particle buffer
        // if we don't do this, garbage values could be in the particle buffer,
        // meaning we'd get some crazy high lifetimes
        particle_buffer->Memset(particle_buffer->Size(), 0);

        // copy bytes into noise buffer
        Array<float> unpacked_floats = noise_map.GetUnpackedFloats();
        AssertThrow(noise_buffer->Size() == unpacked_floats.ByteSize());

        noise_buffer->Copy(unpacked_floats.ByteSize(), unpacked_floats.Data());

        // don't need it anymore
        noise_map = Bitmap<1>();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyParticleSystem)
    : renderer::RenderCommand
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
    : renderer::RenderCommand
{
    GPUBufferRef staging_buffer;
    Handle<Mesh> quad_mesh;

    RENDER_COMMAND(CreateParticleSystemBuffers)(
        GPUBufferRef staging_buffer,
        Handle<Mesh> quad_mesh)
        : staging_buffer(std::move(staging_buffer)),
          quad_mesh(std::move(quad_mesh))
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSystemBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(staging_buffer->Create());

        IndirectDrawCommand empty_draw_command {};
        quad_mesh->GetRenderResource().PopulateIndirectDrawCommand(empty_draw_command);

        // copy zeros to buffer
        staging_buffer->Copy(sizeof(IndirectDrawCommand), &empty_draw_command);

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
    m_render_group.Reset();

    SafeRelease(std::move(m_update_particles));
    SafeRelease(std::move(m_particle_buffer));
    SafeRelease(std::move(m_indirect_buffer));
    SafeRelease(std::move(m_noise_buffer));

    m_shader.Reset();
}

void ParticleSpawner::Init()
{
    if (m_params.texture)
    {
        InitObject(m_params.texture);
        m_params.texture->SetPersistentRenderResourceEnabled(true);
    }

    CreateBuffers();
    CreateRenderGroup();
    CreateComputePipelines();

    SetReady(true);
}

void ParticleSpawner::CreateBuffers()
{
    m_particle_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, m_params.max_particles * sizeof(ParticleShaderData));
    m_indirect_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
    m_noise_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, sizeof(float) * 128 * 128);

    PUSH_RENDER_COMMAND(
        CreateParticleSpawnerBuffers,
        m_particle_buffer,
        m_indirect_buffer,
        m_noise_buffer,
        m_params);
}

void ParticleSpawner::CreateRenderGroup()
{
    m_shader = g_shader_manager->GetOrCreate(NAME("Particle"));
    AssertThrow(m_shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("ParticleDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("ParticlesBuffer"), m_particle_buffer);
        descriptor_set->SetElement(NAME("ParticleTexture"), m_params.texture ? m_params.texture->GetRenderResource().GetImageView() : g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
    }

    DeferCreate(descriptor_table);

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .blend_function = BlendFunction::Additive(),
                .cull_faces = FaceCullMode::FRONT,
                .flags = MaterialAttributeFlags::DEPTH_TEST }),
        descriptor_table,
        RenderGroupFlags::NONE);

    // @FIXME: needs to be per view!
    m_render_group->AddFramebuffer(g_engine->GetCurrentView()->GetGBuffer()->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer());

    AssertThrow(InitObject(m_render_group));
}

void ParticleSpawner::CreateComputePipelines()
{
    ShaderProperties properties;
    properties.Set("HAS_PHYSICS", m_params.has_physics);

    ShaderRef update_particles_shader = g_shader_manager->GetOrCreate(NAME("UpdateParticles"), properties);
    AssertThrow(update_particles_shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = update_particles_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("UpdateParticlesDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("ParticlesBuffer"), m_particle_buffer);
        descriptor_set->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirect_buffer);
        descriptor_set->SetElement(NAME("NoiseBuffer"), m_noise_buffer);
    }

    DeferCreate(descriptor_table);

    m_update_particles = g_rendering_api->MakeComputePipeline(
        update_particles_shader,
        descriptor_table);

    DeferCreate(m_update_particles);
}

#pragma endregion ParticleSpawner

#pragma region ParticleSystem

ParticleSystem::ParticleSystem()
    : HypObject(),
      m_particle_spawners(g_render_thread),
      m_counter(0u)
{
}

ParticleSystem::~ParticleSystem()
{
    SafeRelease(std::move(m_staging_buffer));

    m_quad_mesh.Reset();

    PUSH_RENDER_COMMAND(
        DestroyParticleSystem,
        &m_particle_spawners);

    HYP_SYNC_RENDER();
}

void ParticleSystem::Init()
{
    m_quad_mesh = MeshBuilder::Quad();
    InitObject(m_quad_mesh);

    // Allow Render() to be called directly without a RenderGroup
    m_quad_mesh->SetPersistentRenderResourceEnabled(true);

    CreateBuffers();

    SetReady(true);
}

void ParticleSystem::CreateBuffers()
{
    m_staging_buffer = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STAGING_BUFFER, sizeof(IndirectDrawCommand));

    PUSH_RENDER_COMMAND(
        CreateParticleSystemBuffers,
        m_staging_buffer,
        m_quad_mesh);
}

void ParticleSystem::UpdateParticles(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    if (m_particle_spawners.GetItems().Empty())
    {
        if (m_particle_spawners.HasUpdatesPending())
        {
            m_particle_spawners.UpdateItems();
        }

        return;
    }

    frame->GetCommandList().Add<InsertBarrier>(m_staging_buffer, renderer::ResourceState::COPY_SRC);

    for (auto& spawner : m_particle_spawners)
    {
        const SizeType max_particles = spawner->GetParams().max_particles;

        AssertThrow(spawner->GetIndirectBuffer()->Size() == sizeof(IndirectDrawCommand));
        AssertThrow(spawner->GetParticleBuffer()->Size() >= sizeof(ParticleShaderData) * max_particles);

        frame->GetCommandList().Add<InsertBarrier>(
            spawner->GetIndirectBuffer(),
            renderer::ResourceState::COPY_DST);

        // copy zeros to buffer (to reset instance count)
        frame->GetCommandList().Add<CopyBuffer>(
            m_staging_buffer,
            spawner->GetIndirectBuffer(),
            sizeof(IndirectDrawCommand));

        frame->GetCommandList().Add<InsertBarrier>(
            spawner->GetIndirectBuffer(),
            renderer::ResourceState::INDIRECT_ARG);

        // @FIXME: frustum needs to be added to buffer data
        // if (active_camera != nullptr && !(*active_camera)->GetBufferData().frustum.ContainsBoundingSphere(spawner->GetBoundingSphere())) {
        //     continue;
        // }

        struct
        {
            Vec4f origin;
            float spawn_radius;
            float randomness;
            float avg_lifespan;
            uint32 max_particles;
            float max_particles_sqrt;
            float delta_time;
            uint32 global_counter;
        } push_constants;

        push_constants.origin = Vec4f(spawner->GetParams().origin, spawner->GetParams().start_size);
        push_constants.spawn_radius = spawner->GetParams().radius;
        push_constants.randomness = spawner->GetParams().randomness;
        push_constants.avg_lifespan = spawner->GetParams().lifespan;
        push_constants.max_particles = uint32(max_particles);
        push_constants.max_particles_sqrt = MathUtil::Sqrt(float(max_particles));
        push_constants.delta_time = 0.016f; // TODO! Delta time for particles. we currentl don't have delta time for render thread.
        push_constants.global_counter = m_counter;

        spawner->GetComputePipeline()->SetPushConstants(&push_constants, sizeof(push_constants));

        frame->GetCommandList().Add<BindComputePipeline>(spawner->GetComputePipeline());

        frame->GetCommandList().Add<BindDescriptorTable>(
            spawner->GetComputePipeline()->GetDescriptorTable(),
            spawner->GetComputePipeline(),
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
            frame->GetFrameIndex());

        const uint32 view_descriptor_set_index = spawner->GetComputePipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        if (view_descriptor_set_index != ~0u)
        {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_setup.view->GetDescriptorSets()[frame->GetFrameIndex()],
                spawner->GetComputePipeline(),
                ArrayMap<Name, uint32> {},
                view_descriptor_set_index);
        }

        frame->GetCommandList().Add<DispatchCompute>(
            spawner->GetComputePipeline(),
            Vec3u { uint32((max_particles + 255) / 256), 1, 1 });

        frame->GetCommandList().Add<InsertBarrier>(
            spawner->GetIndirectBuffer(),
            renderer::ResourceState::INDIRECT_ARG);
    }

    ++m_counter;

    // update render-side container after render,
    // so that all objects get initialized before the next render call.
    if (m_particle_spawners.HasUpdatesPending())
    {
        m_particle_spawners.UpdateItems();
    }
}

void ParticleSystem::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
    AssertReady();

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const uint32 frame_index = frame->GetFrameIndex();

    for (const Handle<ParticleSpawner>& particle_spawner : m_particle_spawners.GetItems())
    {
        const GraphicsPipelineRef& pipeline = particle_spawner->GetRenderGroup()->GetPipeline();

        frame->GetCommandList().Add<BindGraphicsPipeline>(pipeline);

        frame->GetCommandList().Add<BindDescriptorTable>(
            pipeline->GetDescriptorTable(),
            pipeline,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
            frame_index);

        const uint32 view_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        if (view_descriptor_set_index != ~0u)
        {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_setup.view->GetDescriptorSets()[frame_index],
                pipeline,
                ArrayMap<Name, uint32> {},
                view_descriptor_set_index);
        }

        m_quad_mesh->GetRenderResource().RenderIndirect(frame->GetCommandList(), particle_spawner->GetIndirectBuffer());
    }
}

#pragma endregion ParticleSystem

} // namespace hyperion
