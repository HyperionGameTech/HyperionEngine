/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ParticleSystem.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <core/HypClassUtils.hpp>

#include <core/util/ForEach.hpp>

#include <math/MathUtil.hpp>
#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(ParticleSpawner);
HYP_DEFINE_CLASS(ParticleSystem);

using renderer::IndirectDrawCommand;
using renderer::Pipeline;
using renderer::Result;
using renderer::GPUBufferType;
using renderer::CommandBufferType;

#pragma region Render commands

struct RENDER_COMMAND(CreateParticleSpawnerBuffers) : renderer::RenderCommand
{
    GPUBufferRef            particle_buffer;
    GPUBufferRef            indirect_buffer;
    GPUBufferRef            noise_buffer;
    ParticleSpawnerParams   params;

    RENDER_COMMAND(CreateParticleSpawnerBuffers)(
        GPUBufferRef particle_buffer,
        GPUBufferRef indirect_buffer,
        GPUBufferRef noise_buffer,
        const ParticleSpawnerParams &params
    ) : particle_buffer(std::move(particle_buffer)),
        indirect_buffer(std::move(indirect_buffer)),
        noise_buffer(std::move(noise_buffer)),
        params(params)
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSpawnerBuffers)() override = default;

    virtual Result operator()() override
    {
        static constexpr uint seed = 0xff;

        SimplexNoiseGenerator noise_generator(seed);
        auto noise_map = noise_generator.CreateBitmap(128, 128, 1024.0f);

        HYPERION_BUBBLE_ERRORS(particle_buffer->Create(
            g_engine->GetGPUDevice(),
            params.max_particles * sizeof(ParticleShaderData)
        ));

        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        HYPERION_BUBBLE_ERRORS(noise_buffer->Create(
            g_engine->GetGPUDevice(),
            noise_map.GetByteSize() * sizeof(float)
        ));

        // copy zeroes into particle buffer
        // if we don't do this, garbage values could be in the particle buffer,
        // meaning we'd get some crazy high lifetimes
        particle_buffer->Memset(
            g_engine->GetGPUDevice(),
            particle_buffer->Size(),
            0x0
        );

        // copy bytes into noise buffer
        Array<float> unpacked_floats = noise_map.GetUnpackedFloats();

        noise_buffer->Copy(
            g_engine->GetGPUDevice(),
            unpacked_floats.ByteSize(),
            unpacked_floats.Data()
        );

        // don't need it anymore
        noise_map = Bitmap<1>();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyParticleSystem) : renderer::RenderCommand
{
    ThreadSafeContainer<ParticleSpawner>    *spawners;

    RENDER_COMMAND(DestroyParticleSystem)(
        ThreadSafeContainer<ParticleSpawner> *spawners
    ) : spawners(spawners)
    {
    }

    virtual ~RENDER_COMMAND(DestroyParticleSystem)() override = default;

    virtual Result operator()() override
    {
        Result result;

        if (spawners->HasUpdatesPending()) {
            spawners->UpdateItems();
        }

        spawners->Clear();

        return result;
    }
};

struct RENDER_COMMAND(CreateParticleSystemBuffers) : renderer::RenderCommand
{
    GPUBufferRef    staging_buffer;
    Handle<Mesh>    quad_mesh;

    RENDER_COMMAND(CreateParticleSystemBuffers)(
        GPUBufferRef staging_buffer,
        Handle<Mesh> quad_mesh
    ) : staging_buffer(std::move(staging_buffer)),
        quad_mesh(std::move(quad_mesh))
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSystemBuffers)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(staging_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        IndirectDrawCommand empty_draw_command { };
        quad_mesh->PopulateIndirectDrawCommand(empty_draw_command);

        // copy zeros to buffer
        staging_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(IndirectDrawCommand),
            &empty_draw_command
        );

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateParticleSystemCommandBuffers) : renderer::RenderCommand
{
    FixedArray<FixedArray<CommandBufferRef, num_async_rendering_command_buffers>, max_frames_in_flight> command_buffers;

    RENDER_COMMAND(CreateParticleSystemCommandBuffers)(
        FixedArray<FixedArray<CommandBufferRef, num_async_rendering_command_buffers>, max_frames_in_flight> command_buffers
    ) : command_buffers(std::move(command_buffers))
    {
    }

    virtual ~RENDER_COMMAND(CreateParticleSystemCommandBuffers)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (uint i = 0; i < uint(command_buffers[frame_index].Size()); i++) {
                AssertThrow(command_buffers[frame_index][i].IsValid());

#ifdef HYP_VULKAN
                command_buffers[frame_index][i]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[i];
#endif

                HYPERION_BUBBLE_ERRORS(command_buffers[frame_index][i]->Create(g_engine->GetGPUInstance()->GetDevice()));
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

ParticleSpawner::ParticleSpawner()
    : m_params { }
{
}

ParticleSpawner::ParticleSpawner(const ParticleSpawnerParams &params)
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
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    if (m_params.texture) {
        InitObject(m_params.texture);
    }

    CreateBuffers();
    CreateRenderGroup();
    CreateComputePipelines();

    HYP_SYNC_RENDER();

    SetReady(true);
}

void ParticleSpawner::Record(CommandBuffer *command_buffer)
{

}

void ParticleSpawner::CreateBuffers()
{
    m_particle_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);
    m_indirect_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::INDIRECT_ARGS_BUFFER);
    m_noise_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateParticleSpawnerBuffers,
        m_particle_buffer,
        m_indirect_buffer,
        m_noise_buffer,
        m_params
    );
}

void ParticleSpawner::CreateRenderGroup()
{
    m_shader = g_shader_manager->GetOrCreate(NAME("Particle"));
    AssertThrow(m_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("ParticleDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("ParticlesBuffer"), m_particle_buffer);
        descriptor_set->SetElement(NAME("ParticleTexture"), m_params.texture
            ? m_params.texture->GetImageView()
            : g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket         = Bucket::BUCKET_TRANSLUCENT,
                .blend_function = BlendFunction::Additive(),
                .cull_faces     = FaceCullMode::FRONT,
                .flags          = MaterialAttributeFlags::DEPTH_TEST
            }
        ),
        descriptor_table,
        RenderGroupFlags::NONE
    );

    m_render_group->AddFramebuffer(g_engine->GetGBuffer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer());

    AssertThrow(InitObject(m_render_group));
}

void ParticleSpawner::CreateComputePipelines()
{
    ShaderProperties properties;
    properties.Set("HAS_PHYSICS", m_params.has_physics);

    ShaderRef update_particles_shader = g_shader_manager->GetOrCreate(NAME("UpdateParticles"), properties);
    AssertThrow(update_particles_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = update_particles_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("UpdateParticlesDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("ParticlesBuffer"), m_particle_buffer);
        descriptor_set->SetElement(NAME("IndirectDrawCommandsBuffer"), m_indirect_buffer);
        descriptor_set->SetElement(NAME("NoiseBuffer"), m_noise_buffer);
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_update_particles = MakeRenderObject<ComputePipeline>(
        update_particles_shader,
        descriptor_table
    );

    DeferCreate(m_update_particles, g_engine->GetGPUDevice());
}

ParticleSystem::ParticleSystem()
    : BasicObject(),
      m_particle_spawners(ThreadName::THREAD_RENDER),
      m_counter(0u)
{
}

ParticleSystem::~ParticleSystem()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (auto &command_buffer : m_command_buffers[frame_index]) {
            SafeRelease(std::move(command_buffer));
        }
    }

    SafeRelease(std::move(m_staging_buffer));

    m_quad_mesh.Reset();
    
    PUSH_RENDER_COMMAND(
        DestroyParticleSystem,
        &m_particle_spawners
    );

    HYP_SYNC_RENDER();
}

void ParticleSystem::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    m_quad_mesh = MeshBuilder::Quad();
    InitObject(m_quad_mesh);

    CreateBuffers();
    CreateCommandBuffers();

    SetReady(true);
}

void ParticleSystem::CreateBuffers()
{
    m_staging_buffer = MakeRenderObject<GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);

    PUSH_RENDER_COMMAND(
        CreateParticleSystemBuffers,
        m_staging_buffer,
        m_quad_mesh
    );
}

void ParticleSystem::CreateCommandBuffers()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (uint i = 0; i < num_async_rendering_command_buffers; i++) {
            m_command_buffers[frame_index][i] = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
        }
    }

    PUSH_RENDER_COMMAND(CreateParticleSystemCommandBuffers, m_command_buffers);
}

void ParticleSystem::UpdateParticles(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    if (m_particle_spawners.GetItems().Empty()) {
        if (m_particle_spawners.HasUpdatesPending()) {
            m_particle_spawners.UpdateItems();
        }

        return;
    }

    m_staging_buffer->InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );

    for (auto &spawner : m_particle_spawners) {
        const SizeType max_particles = spawner->GetParams().max_particles;

        AssertThrow(spawner->GetIndirectBuffer()->Size() == sizeof(IndirectDrawCommand));
        AssertThrow(spawner->GetParticleBuffer()->Size() >= sizeof(ParticleShaderData) * max_particles);

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::COPY_DST
        );

        // copy zeros to buffer (to reset instance count)
        spawner->GetIndirectBuffer()->CopyFrom(
            frame->GetCommandBuffer(),
            m_staging_buffer,
            sizeof(IndirectDrawCommand)
        );

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );

        if (!g_engine->GetRenderState().GetCamera().camera.frustum.ContainsBoundingSphere(spawner->GetBoundingSphere())) {
            continue;
        }

        struct alignas(128)
        {
            Vec4f   origin;
            float   spawn_radius;
            float   randomness;
            float   avg_lifespan;
            uint32  max_particles;
            float   max_particles_sqrt;
            float   delta_time;
            uint32  global_counter;
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

        spawner->GetComputePipeline()->Bind(frame->GetCommandBuffer());

        spawner->GetComputePipeline()->GetDescriptorTable()->Bind(
            frame,
            spawner->GetComputePipeline(),
            {
                {
                    NAME("Scene"),
                    {
                        {
                            { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    }
                }
            }
        );

        spawner->GetComputePipeline()->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                uint32((max_particles + 255) / 256), 1, 1
            }
        );

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );
    }

    ++m_counter;

    // update render-side container after render,
    // so that all objects get initialized before the next render call.
    if (m_particle_spawners.HasUpdatesPending()) {
        m_particle_spawners.UpdateItems();
    }
}

void ParticleSystem::Render(Frame *frame)
{
    AssertReady();

    const uint frame_index = frame->GetFrameIndex();

    FixedArray<uint, num_async_rendering_command_buffers> command_buffers_recorded_states { };
    
    ForEachInBatches(
        m_particle_spawners.GetItems(),
        num_async_rendering_command_buffers,
        [this, &command_buffers_recorded_states, frame_index](const Handle<ParticleSpawner> &particle_spawner, uint index, uint batch_index) {
            const GraphicsPipelineRef &pipeline = particle_spawner->GetRenderGroup()->GetPipeline();

            m_command_buffers[frame_index][batch_index]->Record(
                g_engine->GetGPUDevice(),
                pipeline->GetRenderPass(),
                [&](CommandBuffer *secondary)
                {
                    pipeline->Bind(secondary);

                    pipeline->GetDescriptorTable()->Bind(
                        secondary,
                        frame_index,
                        pipeline,
                        {
                            {
                                NAME("Scene"),
                                {
                                    { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                                    { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                                    { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                                    { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                                    { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                                }
                            }
                        }
                    );

                    m_quad_mesh->RenderIndirect(
                        secondary,
                        particle_spawner->GetIndirectBuffer().Get()
                    );

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[batch_index] = 1u;
        }
    );

    const uint num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (uint i = 0; i < num_recorded_command_buffers; i++) {
        m_command_buffers[frame_index][i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }
}

} // namespace hyperion
