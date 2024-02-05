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
using renderer::Result;
using renderer::GPUBufferType;
using renderer::CommandBufferType;

struct RENDER_COMMAND(CreateParticleSpawnerBuffers) : renderer::RenderCommand
{
    GPUBufferRef particle_buffer;
    GPUBufferRef indirect_buffer;
    GPUBufferRef noise_buffer;
    ParticleSpawnerParams params;

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

    virtual Result operator()()
    {
        static constexpr UInt seed = 0xff;

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
            noise_map.GetByteSize() * sizeof(Float)
        ));

        // copy zeroes into particle buffer
        // if we don't do this, garbage values could be in the particle buffer,
        // meaning we'd get some crazy high lifetimes
        particle_buffer->Memset(
            g_engine->GetGPUDevice(),
            particle_buffer->size,
            0x00
        );

        // copy bytes into noise buffer
        Array<Float> unpacked_floats;
        noise_map.GetUnpackedFloats(unpacked_floats);
        AssertThrow(noise_map.GetByteSize() == unpacked_floats.Size());

        noise_buffer->Copy(
            g_engine->GetGPUDevice(),
            unpacked_floats.Size() * sizeof(Float),
            unpacked_floats.Data()
        );

        // don't need it anymore
        noise_map = Bitmap<1>();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateParticleDescriptors) : renderer::RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    RENDER_COMMAND(CreateParticleDescriptors)(
        FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets
    ) : descriptor_sets(std::move(descriptor_sets))
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            ));
        }

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

    virtual Result operator()()
    {
        auto result = Result::OK;

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
    Mesh            *quad_mesh;

    RENDER_COMMAND(CreateParticleSystemBuffers)(
        GPUBufferRef staging_buffer,
        Mesh *quad_mesh
    ) : staging_buffer(std::move(staging_buffer)),
        quad_mesh(quad_mesh)
    {
    }

    virtual Result operator()()
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
                command_buffers[frame_index][i].Reset(new CommandBuffer(CommandBufferType::COMMAND_BUFFER_SECONDARY));
    
                HYPERION_BUBBLE_ERRORS(command_buffers[frame_index][i]->Create(
                    g_engine->GetGPUInstance()->GetDevice(),
                    g_engine->GetGPUInstance()->GetGraphicsCommandPool(i)
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
    if (IsInitCalled()) {
        m_render_group.Reset();
        m_update_particles.Reset();

        SafeRelease(std::move(m_particle_buffer));
        SafeRelease(std::move(m_indirect_buffer));
        SafeRelease(std::move(m_noise_buffer));

        SafeRelease(std::move(m_descriptor_sets));

        m_shader.Reset();
    }
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
    CreateShader();
    CreateDescriptorSets();
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

void ParticleSpawner::CreateShader()
{
    m_shader = g_shader_manager->GetOrCreate(HYP_NAME(Particle));

    InitObject(m_shader);
}

void ParticleSpawner::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = MakeRenderObject<DescriptorSet>();

        // particle data
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetElementBuffer(0, m_particle_buffer);

        // indirect rendering data
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
            ->SetElementBuffer(0, m_indirect_buffer);

        // noise buffer
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetElementBuffer(0, m_noise_buffer);

        // scene
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetElementBuffer<SceneShaderData>(0, g_engine->GetRenderData()->scenes.GetBuffer());
    
        // camera
        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(5)
            ->SetElementBuffer<CameraShaderData>(0, g_engine->GetRenderData()->cameras.GetBuffer());

        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::ImageDescriptor>(6)
            ->SetElementSRV(0, m_params.texture
                ? m_params.texture->GetImageView()
                : g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::SamplerDescriptor>(7)
            ->SetElementSampler(0, g_engine->GetPlaceholderData()->GetSamplerLinear());

        m_descriptor_sets[frame_index]
            ->AddDescriptor<renderer::SamplerDescriptor>(8)
            ->SetElementSampler(0, g_engine->GetPlaceholderData()->GetSamplerNearest());

        // gbuffer normals texture
        m_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(9)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_NORMALS)->GetImageView());

        // gbuffer depth texture
        m_descriptor_sets[frame_index]
            ->AddDescriptor<ImageDescriptor>(10)
            ->SetElementSRV(0, g_engine->GetDeferredSystem().Get(BUCKET_OPAQUE).GetGBufferAttachment(GBUFFER_RESOURCE_DEPTH)->GetImageView());
    }

    PUSH_RENDER_COMMAND(
        CreateParticleDescriptors,
        m_descriptor_sets
    );
}

void ParticleSpawner::CreateRenderGroup()
{
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

    m_render_group->AddFramebuffer(Handle<Framebuffer>(g_engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer()));

    // do not use global descriptor sets for this renderer -- we will just use our own local ones
    m_render_group->GetPipeline()->SetUsedDescriptorSets(Array<DescriptorSetRef> {
        m_descriptor_sets[0]
    });

    AssertThrow(InitObject(m_render_group));
}

void ParticleSpawner::CreateComputePipelines()
{
    ShaderProperties properties;
    properties.Set("HAS_PHYSICS", m_params.has_physics);

    m_update_particles = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(
            HYP_NAME(UpdateParticles),
            properties
        ),
        Array<DescriptorSetRef> { m_descriptor_sets[0] }
    );

    InitObject(m_update_particles);
}

ParticleSystem::ParticleSystem()
    : BasicObject(),
      m_particle_spawners(THREAD_RENDER),
      m_counter(0u)
{
}

ParticleSystem::~ParticleSystem()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (auto &command_buffer : m_command_buffers[frame_index]) {
            g_safe_deleter->SafeRelease(std::move(command_buffer));
        }
    }

    SafeRelease(std::move(m_staging_buffer));

    m_quad_mesh.Reset();

    Teardown();
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

    OnTeardown([this]() {
        PUSH_RENDER_COMMAND(
            DestroyParticleSystem, 
            &m_particle_spawners
        );
        
        HYP_SYNC_RENDER();
        
        SetReady(false);
    });
}

void ParticleSystem::CreateBuffers()
{
    m_staging_buffer = MakeRenderObject<renderer::GPUBuffer>(renderer::GPUBufferType::STAGING_BUFFER);

    PUSH_RENDER_COMMAND(CreateParticleSystemBuffers, 
        m_staging_buffer,
        m_quad_mesh.Get()
    );
}

void ParticleSystem::CreateCommandBuffers()
{
    PUSH_RENDER_COMMAND(CreateParticleSystemCommandBuffers, 
        m_command_buffers
    );
}

void ParticleSystem::UpdateParticles(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
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

        AssertThrow(spawner->GetIndirectBuffer()->size == sizeof(IndirectDrawCommand));
        AssertThrow(spawner->GetParticleBuffer()->size >= sizeof(ParticleShaderData) * max_particles);

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
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            spawner->GetComputePipeline()->GetPipeline(),
            spawner->GetDescriptorSets()[frame->GetFrameIndex()],
            0,
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
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

    // update render-side container after render,
    // so that all objects get initialized before the next render call.
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
    TaskSystem::GetInstance().ParallelForEach(
        THREAD_POOL_RENDER,
        m_particle_spawners.GetItems(),
        [this, &command_buffers_recorded_states, frame_index](const Handle<ParticleSpawner> &particle_spawner, UInt index, UInt batch_index) {
            const GraphicsPipelineRef &pipeline = particle_spawner->GetRenderGroup()->GetPipeline();

            m_command_buffers[frame_index][batch_index]->Record(
                g_engine->GetGPUDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary) {
                    pipeline->Bind(secondary);

                    secondary->BindDescriptorSet(
                        g_engine->GetGPUInstance()->GetDescriptorPool(),
                        pipeline,
                        particle_spawner->GetDescriptorSets()[frame_index],
                        0,
                        FixedArray {
                            HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                            HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
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
