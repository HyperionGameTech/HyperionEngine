#include "ParticleSystem.hpp"

#include <Engine.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <util/MeshBuilder.hpp>
#include <math/MathUtil.hpp>
#include <math/Color.hpp>
#include <util/NoiseFactory.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;
using renderer::Pipeline;
using renderer::StagingBuffer;
using renderer::Result;

struct RENDER_COMMAND(CreateParticleSpawnerBuffers) : RenderCommand
{
    StorageBuffer *particle_buffer;
    IndirectBuffer *indirect_buffer;
    StorageBuffer *noise_buffer;
    ParticleSpawnerParams params;

    RENDER_COMMAND(CreateParticleSpawnerBuffers)(
        StorageBuffer *particle_buffer,
        IndirectBuffer *indirect_buffer,
        StorageBuffer *noise_buffer,
        const ParticleSpawnerParams &params
    ) : particle_buffer(particle_buffer),
        indirect_buffer(indirect_buffer),
        noise_buffer(noise_buffer),
        params(params)
    {
    }

    virtual Result operator()()
    {
        static constexpr UInt seed = 0xff;

        SimplexNoiseGenerator noise_generator(seed);
        auto noise_map = noise_generator.CreateBitmap(128, 128, 1024.0f);

        HYPERION_BUBBLE_ERRORS(particle_buffer->Create(
            Engine::Get()->GetGPUDevice(),
            params.max_particles * sizeof(ParticleShaderData)
        ));

        HYPERION_BUBBLE_ERRORS(indirect_buffer->Create(
            Engine::Get()->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        HYPERION_BUBBLE_ERRORS(noise_buffer->Create(
            Engine::Get()->GetGPUDevice(),
            noise_map.GetByteSize() * sizeof(Float)
        ));

        // copy zeroes into particle buffer
        // if we don't do this, garbage values could be in the particle buffer,
        // meaning we'd get some crazy high lifetimes
        particle_buffer->Memset(
            Engine::Get()->GetGPUDevice(),
            particle_buffer->size,
            0x00
        );

        // copy bytes into noise buffer
        Array<Float> unpacked_floats;
        noise_map.GetUnpackedFloats(unpacked_floats);
        AssertThrow(noise_map.GetByteSize() == unpacked_floats.Size());

        noise_buffer->Copy(
            Engine::Get()->GetGPUDevice(),
            unpacked_floats.Size() * sizeof(Float),
            unpacked_floats.Data()
        );

        // don't need it anymore
        noise_map = Bitmap<1>();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateParticleDescriptors) : RenderCommand
{
    renderer::DescriptorSet *descriptor_sets;

    RENDER_COMMAND(CreateParticleDescriptors)(
        renderer::DescriptorSet *descriptor_sets
    ) : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index].Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyParticleSystem) : RenderCommand
{
    StagingBuffer *staging_buffer;
    ThreadSafeContainer<ParticleSpawner> *spawners;

    RENDER_COMMAND(DestroyParticleSystem)(
        StagingBuffer *staging_buffer,
        ThreadSafeContainer<ParticleSpawner> *spawners
    ) : staging_buffer(staging_buffer),
        spawners(spawners)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        HYPERION_PASS_ERRORS(
            staging_buffer->Destroy(Engine::Get()->GetGPUDevice()),
            result
        );

        if (spawners->HasUpdatesPending()) {
            spawners->UpdateItems();
        }

        spawners->Clear();

        return result;
    }
};

struct RENDER_COMMAND(DestroyParticleDescriptors) : RenderCommand
{
    renderer::DescriptorSet *descriptor_sets;

    RENDER_COMMAND(DestroyParticleDescriptors)(
        renderer::DescriptorSet *descriptor_sets
    ) : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_PASS_ERRORS(
                descriptor_sets[frame_index].Destroy(Engine::Get()->GetGPUDevice()),
                result
            );
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateParticleSystemBuffers) : RenderCommand
{
    renderer::StagingBuffer *staging_buffer;
    Mesh *quad_mesh;

    RENDER_COMMAND(CreateParticleSystemBuffers)(
        renderer::StagingBuffer *staging_buffer,
        Mesh *quad_mesh
    ) : staging_buffer(staging_buffer),
        quad_mesh(quad_mesh)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(staging_buffer->Create(
            Engine::Get()->GetGPUDevice(),
            sizeof(IndirectDrawCommand)
        ));

        IndirectDrawCommand empty_draw_command { };
        quad_mesh->PopulateIndirectDrawCommand(empty_draw_command);

        // copy zeros to buffer
        staging_buffer->Copy(
            Engine::Get()->GetGPUDevice(),
            sizeof(IndirectDrawCommand),
            &empty_draw_command
        );

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateParticleSystemCommandBuffers) : RenderCommand
{
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> &command_buffers;

    RENDER_COMMAND(CreateParticleSystemCommandBuffers)(
        FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> &command_buffers
    ) : command_buffers(command_buffers)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (UInt i = 0; i < UInt(command_buffers[frame_index].Size()); i++) {
                command_buffers[frame_index][i].Reset(new CommandBuffer(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY));
    
                HYPERION_BUBBLE_ERRORS(command_buffers[frame_index][i]->Create(
                    Engine::Get()->GetGPUInstance()->GetDevice(),
                    Engine::Get()->GetGPUInstance()->GetGraphicsCommandPool(i)
                ));
            }
        }

        HYPERION_RETURN_OK;
    }
};

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
    Teardown();
}

void ParticleSpawner::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    if (m_params.texture) {
        InitObject(m_params.texture);
    }

    CreateBuffers();
    CreateShader();
    CreateDescriptorSets();
    CreateRenderGroup();
    CreateComputePipelines();

    HYP_SYNC_RENDER();

    OnTeardown([this](...) {
        m_render_group.Reset();
        m_update_particles.Reset();

        Engine::Get()->SafeRelease(std::move(m_particle_buffer));
        Engine::Get()->SafeRelease(std::move(m_indirect_buffer));
        Engine::Get()->SafeRelease(std::move(m_noise_buffer));
        Engine::Get()->SafeReleaseHandle(std::move(m_params.texture));
        Engine::Get()->SafeReleaseHandle(std::move(m_shader));

        RenderCommands::Push<RENDER_COMMAND(DestroyParticleDescriptors)>(
            m_descriptor_sets.Data()
        );

        HYP_SYNC_RENDER();
    });
}

void ParticleSpawner::Record(CommandBuffer *command_buffer)
{

}

void ParticleSpawner::CreateBuffers()
{
    m_particle_buffer = UniquePtr<StorageBuffer>::Construct();
    m_indirect_buffer = UniquePtr<IndirectBuffer>::Construct();
    m_noise_buffer = UniquePtr<StorageBuffer>::Construct();

    RenderCommands::Push<RENDER_COMMAND(CreateParticleSpawnerBuffers)>(
        m_particle_buffer.Get(),
        m_indirect_buffer.Get(),
        m_noise_buffer.Get(),
        m_params
    );
}

void ParticleSpawner::CreateShader()
{
    m_shader = Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(Particle));
    InitObject(m_shader);
}

void ParticleSpawner::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // particle data
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, m_particle_buffer.Get());

        // indirect rendering data
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::StorageBufferDescriptor>(1)
            ->SetElementBuffer(0, m_indirect_buffer.Get());

        // noise buffer
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_noise_buffer.Get());

        // scene
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetElementBuffer<SceneShaderData>(0, Engine::Get()->GetRenderData()->scenes.GetBuffers()[frame_index].get());
    
        // camera
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::DynamicUniformBufferDescriptor>(5)
            ->SetElementBuffer<CameraShaderData>(0, Engine::Get()->GetRenderData()->cameras.GetBuffers()[frame_index].get());

        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::ImageDescriptor>(6)
            ->SetElementSRV(0, m_params.texture
                ? m_params.texture->GetImageView()
                : &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8());

        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::SamplerDescriptor>(7)
            ->SetElementSampler(0, m_params.texture
                ? m_params.texture->GetSampler()
                : &Engine::Get()->GetPlaceholderData().GetSamplerLinear());

        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::SamplerDescriptor>(8)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        // gbuffer normals texture
        m_descriptor_sets[frame_index]
            .AddDescriptor<ImageDescriptor>(9)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        // gbuffer depth texture
        m_descriptor_sets[frame_index]
            .AddDescriptor<ImageDescriptor>(10)
            ->SetElementSRV(0, Engine::Get()->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());
    }

    RenderCommands::Push<RENDER_COMMAND(CreateParticleDescriptors)>(
        m_descriptor_sets.Data()
    );
}

void ParticleSpawner::CreateRenderGroup()
{
    // Not using Engine::FindOrCreateRenderGroup because we want to use
    // our own descriptor sets which will be destroyed when this object is destroyed.
    // we don't want any other objects to use our RenderGroup then!
    m_render_group = CreateObject<RenderGroup>(
        Handle<Shader>(m_shader),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .blend_mode = BlendMode::ADDITIVE,
                .cull_faces = FaceCullMode::FRONT,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST
            }
        )
    );

    m_render_group->AddFramebuffer(Handle<Framebuffer>(Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer()));

    // do not use global descriptor sets for this renderer -- we will just use our own local ones
    m_render_group->GetPipeline()->SetUsedDescriptorSets(Array<const DescriptorSet *> {
        &m_descriptor_sets[0]
    });

    AssertThrow(InitObject(m_render_group));
}

void ParticleSpawner::CreateComputePipelines()
{
    m_update_particles = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(UpdateParticles)),
        Array<const DescriptorSet *> { &m_descriptor_sets[0] }
    );

    InitObject(m_update_particles);
}

ParticleSystem::ParticleSystem()
    : EngineComponentBase(),
      m_particle_spawners(THREAD_RENDER),
      m_counter(0u)
{
}

ParticleSystem::~ParticleSystem()
{
    Teardown();
}

void ParticleSystem::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    m_quad_mesh = MeshBuilder::Quad();
    InitObject(m_quad_mesh);

    CreateBuffers();
    CreateCommandBuffers();
    
    SetReady(true);

    OnTeardown([this]() {
        Engine::Get()->SafeReleaseHandle<Mesh>(std::move(m_quad_mesh));

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &command_buffer : m_command_buffers[frame_index]) {
                Engine::Get()->SafeRelease(std::move(command_buffer));
            }
        }

        RenderCommands::Push<RENDER_COMMAND(DestroyParticleSystem)>(
            &m_staging_buffer,
            &m_particle_spawners
        );
        
        HYP_SYNC_RENDER();
        
        SetReady(false);
    });
}

void ParticleSystem::CreateBuffers()
{
    RenderCommands::Push<RENDER_COMMAND(CreateParticleSystemBuffers)>(
        &m_staging_buffer,
        m_quad_mesh.Get()
    );
}

void ParticleSystem::CreateCommandBuffers()
{
    RenderCommands::Push<RENDER_COMMAND(CreateParticleSystemCommandBuffers)>(
        m_command_buffers
    );
}

void ParticleSystem::UpdateParticles(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_particle_spawners.HasUpdatesPending()) {
        m_particle_spawners.UpdateItems();
    }

    if (m_particle_spawners.GetItems().Empty()) {
        return;
    }

    m_staging_buffer.InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );

    for (auto &spawner : m_particle_spawners) {
        const SizeType max_particles = spawner->GetParams().max_particles;

        AssertThrow(spawner->GetIndirectBuffer()->size == sizeof(IndirectDrawCommand));
        AssertThrow(spawner->GetParticleBuffer()->size >= sizeof(ParticleShaderData) * max_particles);

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::COPY_DST
        );

        // copy zeros to buffer (to reset instance count)
        spawner->GetIndirectBuffer()->CopyFrom(
            frame->GetCommandBuffer(),
            &m_staging_buffer,
            sizeof(IndirectDrawCommand)
        );

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );

        if (!Engine::Get()->GetRenderState().GetCamera().camera.frustum.ContainsBoundingSphere(spawner->GetBoundingSphere())) {
            continue;
        }

        spawner->GetComputePipeline()->GetPipeline()->Bind(frame->GetCommandBuffer());

        spawner->GetComputePipeline()->GetPipeline()->SetPushConstants(Pipeline::PushConstantData {
            .particle_spawner_data = {
                .origin             = ShaderVec4<Float32>(Vector4(spawner->GetParams().origin, spawner->GetParams().start_size)),
                .spawn_radius       = spawner->GetParams().radius,
                .randomness         = spawner->GetParams().randomness,
                .avg_lifespan       = spawner->GetParams().lifespan,
                .max_particles      = UInt32(max_particles),
                .max_particles_sqrt = MathUtil::Sqrt(Float(max_particles)),
                .delta_time         = 0.016f, // TODO! Delta time for particles. we currentl don't have delta time for render thread.
                .global_counter     = m_counter
            }
        });

        spawner->GetComputePipeline()->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

        frame->GetCommandBuffer()->BindDescriptorSet(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            spawner->GetComputePipeline()->GetPipeline(),
            &spawner->GetDescriptorSets()[frame->GetFrameIndex()],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->GetRenderState().GetScene().id.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
            }
        );

        spawner->GetComputePipeline()->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                UInt32((max_particles + 255) / 256), 1, 1
            }
        );

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );
    }

    ++m_counter;

    if (m_particle_spawners.HasUpdatesPending()) {
        m_particle_spawners.UpdateItems();
    }
}

void ParticleSystem::Render(Frame *frame)
{
    AssertReady();

    const UInt frame_index = frame->GetFrameIndex();

    FixedArray<UInt, num_async_rendering_command_buffers> command_buffers_recorded_states { };
    
    // always run renderer items as HIGH priority,
    // so we do not lock up because we're waiting for a large process to
    // complete in the same thread
    Engine::Get()->task_system.ParallelForEach(
        TASK_PRIORITY_HIGH,
        m_particle_spawners.GetItems(),
        [this, &command_buffers_recorded_states, frame_index](const Handle<ParticleSpawner> &particle_spawner, UInt index, UInt batch_index) {
            auto *pipeline = particle_spawner->GetRenderGroup()->GetPipeline();

            m_command_buffers[frame_index][batch_index]->Record(
                Engine::Get()->GetGPUDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary) {
                    pipeline->Bind(secondary);

                    secondary->BindDescriptorSet(
                        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                        pipeline,
                        &particle_spawner->GetDescriptorSets()[frame_index],
                        0,
                        FixedArray {
                            HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->GetRenderState().GetScene().id.ToIndex()),
                            HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
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

    const UInt num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (UInt i = 0; i < num_recorded_command_buffers; i++) {
        m_command_buffers[frame_index][i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }
}

} // namespace hyperion::v2
