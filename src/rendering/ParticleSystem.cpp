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

// struct RENDER_COMMAND(CreateParticleBuffers) : RenderCommandBase2
// {

// };

struct alignas(16) ParticleShaderData
{
    ShaderVec4<Float32> position;
    ShaderVec4<Float32> velocity;
    Float32 lifetime;
    Color color;
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

void ParticleSpawner::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    if (m_params.texture) {
        engine->InitObject(m_params.texture);
    }

    CreateNoiseMap();
    CreateBuffers();
    CreateShader();
    CreateDescriptorSets();
    CreateRendererInstance();
    CreateComputePipelines();

    OnTeardown([this](...) {
        m_renderer_instance.Reset();
        m_update_particles.Reset();

        GetEngine()->SafeRelease(std::move(m_particle_buffer));
        GetEngine()->SafeRelease(std::move(m_indirect_buffer));
        GetEngine()->SafeRelease(std::move(m_noise_buffer));
        GetEngine()->SafeReleaseHandle(std::move(m_params.texture));
        GetEngine()->SafeReleaseHandle(std::move(m_shader));

        GetEngine()->GetRenderScheduler().Enqueue([this](...) {
            auto *engine = GetEngine();
            auto result = Result::OK;

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                HYPERION_PASS_ERRORS(
                    m_descriptor_sets[frame_index].Destroy(engine->GetDevice()),
                    result
                );
            }

            return result;
        });

        HYP_FLUSH_RENDER_QUEUE(GetEngine());
    });
}

void ParticleSpawner::Record(Engine *engine, CommandBuffer *command_buffer)
{

}

void ParticleSpawner::CreateNoiseMap()
{
    static constexpr UInt seed = 0xff;

    SimplexNoiseGenerator noise_generator(seed);
    m_noise_map = noise_generator.CreateBitmap(128, 128, 1024.0f);
}

void ParticleSpawner::CreateBuffers()
{
    m_particle_buffer = UniquePtr<StorageBuffer>::Construct();
    m_indirect_buffer = UniquePtr<IndirectBuffer>::Construct();
    m_noise_buffer = UniquePtr<StorageBuffer>::Construct();
    
    GetEngine()->GetRenderScheduler().Enqueue([this](...) {
        HYPERION_BUBBLE_ERRORS(m_particle_buffer->Create(
            GetEngine()->GetDevice(),
            m_params.max_particles * sizeof(ParticleShaderData)
        ));

        HYPERION_BUBBLE_ERRORS(m_indirect_buffer->Create(
            GetEngine()->GetDevice(),
            sizeof(IndirectDrawCommand)
        ));

        HYPERION_BUBBLE_ERRORS(m_noise_buffer->Create(
            GetEngine()->GetDevice(),
            m_noise_map.GetByteSize() * sizeof(Float)
        ));

        // copy zeroes into particle buffer
        // if we don't do this, garbage values could be in the particle buffer,
        // meaning we'd get some crazy high lifetimes
        m_particle_buffer->Memset(
            GetEngine()->GetDevice(),
            m_particle_buffer->size,
            0u
        );

        // copy bytes into noise buffer
        Array<Float> unpacked_floats;
        m_noise_map.GetUnpackedFloats(unpacked_floats);

        AssertThrow(m_noise_map.GetByteSize() == unpacked_floats.Size());

        m_noise_buffer->Copy(
            GetEngine()->GetDevice(),
            unpacked_floats.Size() * sizeof(Float),
            unpacked_floats.Data()
        );

        // don't need it anymore
        m_noise_map = Bitmap<1>();

        HYPERION_RETURN_OK;
    });
}

void ParticleSpawner::CreateShader()
{
    m_shader = GetEngine()->CreateHandle<Shader>(GetEngine()->GetShaderCompiler().GetCompiledShader("Particle"));
    GetEngine()->InitObject(m_shader);
}

void ParticleSpawner::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        // particle data
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_particle_buffer.Get()
            });

        // indirect rendering data
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::StorageBufferDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_indirect_buffer.Get()
            });

        // noise buffer
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::StorageBufferDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_noise_buffer.Get()
            });

        // scene data (for camera matrices)
        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::DynamicStorageBufferDescriptor>(4)
            ->SetSubDescriptor({
                .buffer = GetEngine()->GetRenderData()->scenes.GetBuffers()[frame_index].get(),
                .range = static_cast<UInt32>(sizeof(SceneShaderData))
            });

        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::ImageDescriptor>(6)
            ->SetSubDescriptor({
                .image_view = m_params.texture
                    ? &m_params.texture->GetImageView()
                    : &GetEngine()->GetPlaceholderData().GetImageView2D1x1R8()
            });

        m_descriptor_sets[frame_index]
            .AddDescriptor<renderer::SamplerDescriptor>(7)
            ->SetSubDescriptor({
                .sampler = m_params.texture
                    ? &m_params.texture->GetSampler()
                    : &GetEngine()->GetPlaceholderData().GetSamplerLinear()
            });
    }

    GetEngine()->GetRenderScheduler().Enqueue([this](...) {
        auto *engine = GetEngine();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(m_descriptor_sets[frame_index].Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    });
}

void ParticleSpawner::CreateRendererInstance()
{
    // Not using Engine::FindOrCreateRendererInstance because we want to use
    // our own descriptor sets which will be destroyed when this object is destroyed.
    // we don't want any other objects to use our RendererInstance then!
    m_renderer_instance = GetEngine()->CreateHandle<RendererInstance>(
        Handle<Shader>(m_shader),
        Handle<RenderPass>(GetEngine()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass()),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST
                    | MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING
            }
        )
    );

    for (auto &framebuffer : GetEngine()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()) {
        m_renderer_instance->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }

    // do not use global descriptor sets for this renderer -- we will just use our own local ones
    m_renderer_instance->GetPipeline()->SetUsedDescriptorSets(Array<const DescriptorSet *> {
        &m_descriptor_sets[0]
    });

    AssertThrow(GetEngine()->InitObject(m_renderer_instance));
}

void ParticleSpawner::CreateComputePipelines()
{
    m_update_particles = GetEngine()->CreateHandle<ComputePipeline>(
        GetEngine()->CreateHandle<Shader>(GetEngine()->GetShaderCompiler().GetCompiledShader("UpdateParticles")),
        Array<const DescriptorSet *> { &m_descriptor_sets[0] }
    );

    GetEngine()->InitObject(m_update_particles);
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

void ParticleSystem::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    m_quad_mesh = engine->CreateHandle<Mesh>(MeshBuilder::Quad());
    engine->InitObject(m_quad_mesh);

    CreateBuffers();
    CreateCommandBuffers();
    
    SetReady(true);

    OnTeardown([this]() {
        auto *engine = GetEngine();

        engine->SafeReleaseHandle<Mesh>(std::move(m_quad_mesh));

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &command_buffer : m_command_buffers[frame_index]) {
                engine->SafeRelease(std::move(command_buffer));
            }
        }

        engine->GetRenderScheduler().Enqueue([this, engine](...) {
            auto result = Result::OK;

            HYPERION_PASS_ERRORS(
                m_staging_buffer.Destroy(engine->GetDevice()),
                result
            );

            if (m_particle_spawners.HasUpdatesPending()) {
                m_particle_spawners.UpdateItems(engine);
            }

            m_particle_spawners.Clear();

            return result;
        });
        
        HYP_FLUSH_RENDER_QUEUE(engine);
        
        SetReady(false);
    });
}

void ParticleSystem::CreateBuffers()
{
    GetEngine()->GetRenderScheduler().Enqueue([this](...) {
        HYPERION_BUBBLE_ERRORS(m_staging_buffer.Create(
            GetEngine()->GetDevice(),
            sizeof(IndirectDrawCommand)
        ));

        IndirectDrawCommand empty_draw_command { };
        m_quad_mesh->PopulateIndirectDrawCommand(empty_draw_command);

        // copy zeros to buffer
        m_staging_buffer.Copy(
            GetEngine()->GetDevice(),
            sizeof(IndirectDrawCommand),
            &empty_draw_command
        );

        HYPERION_RETURN_OK;
    });
}

void ParticleSystem::CreateCommandBuffers()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        for (auto &command_buffer : m_command_buffers[frame_index]) {
            command_buffer.Reset(new CommandBuffer(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY));
        }
    }

    GetEngine()->GetRenderScheduler().Enqueue([this](...) {
        auto *engine = GetEngine();

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (UInt i = 0; i < static_cast<UInt>(m_command_buffers[frame_index].Size()); i++) {
                HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index][i]->Create(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool(i)
                ));
            }
        }

        HYPERION_RETURN_OK;
    });
}

void ParticleSystem::UpdateParticles(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_particle_spawners.GetItems().Empty()) {
        if (m_particle_spawners.HasUpdatesPending()) {
            m_particle_spawners.UpdateItems(engine);
        }

        return;
    }

    const auto scene_id = engine->render_state.GetScene().id;
    const auto scene_index = scene_id ? scene_id.value - 1 : 0u;

    m_staging_buffer.InsertBarrier(
        frame->GetCommandBuffer(),
        renderer::ResourceState::COPY_SRC
    );

    for (auto &spawner : m_particle_spawners) {
        const auto aabb = spawner->GetEstimatedAABB();
        const auto max_particles = spawner->GetParams().max_particles;

        AssertThrow(spawner->GetIndirectBuffer()->size == sizeof(IndirectDrawCommand));
        AssertThrow(spawner->GetParticleBuffer()->size >= sizeof(ParticleShaderData) * max_particles);

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::COPY_DST
        );

        // copy zeros to buffer
        spawner->GetIndirectBuffer()->CopyFrom(
            frame->GetCommandBuffer(),
            &m_staging_buffer,
            sizeof(IndirectDrawCommand)
        );

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );

        if (!engine->render_state.GetScene().scene.camera.frustum.ContainsAABB(aabb)) {
            continue;
        }

        spawner->GetComputePipeline()->GetPipeline()->Bind(frame->GetCommandBuffer());

        spawner->GetComputePipeline()->GetPipeline()->SetPushConstants(Pipeline::PushConstantData {
            .particle_spawner_data = {
                .origin = ShaderVec4<Float32>(Vector4(spawner->GetParams().origin, 1.0f)),
                .spawn_radius = spawner->GetParams().radius,
                .randomness = spawner->GetParams().randomness,
                .avg_lifespan = spawner->GetParams().lifespan,
                .max_particles = static_cast<UInt32>(max_particles),
                .max_particles_sqrt = MathUtil::Sqrt(static_cast<Float>(max_particles)),
                .delta_time = 0.016f, // TODO! Delta time for particles. we currentl don't have delta time for render thread.
                .global_counter = m_counter
            }
        });

        spawner->GetComputePipeline()->GetPipeline()->SubmitPushConstants(frame->GetCommandBuffer());

        frame->GetCommandBuffer()->BindDescriptorSet(
            engine->GetInstance()->GetDescriptorPool(),
            spawner->GetComputePipeline()->GetPipeline(),
            &spawner->GetDescriptorSets()[frame->GetFrameIndex()],
            0,
            FixedArray { static_cast<UInt32>(scene_index * sizeof(SceneShaderData)) }
        );

        spawner->GetComputePipeline()->GetPipeline()->Dispatch(
            frame->GetCommandBuffer(),
            Extent3D {
                static_cast<UInt32>((max_particles + 255) / 256), 1, 1
            }
        );

        spawner->GetIndirectBuffer()->InsertBarrier(
            frame->GetCommandBuffer(),
            renderer::ResourceState::INDIRECT_ARG
        );
    }

    ++m_counter;

    if (m_particle_spawners.HasUpdatesPending()) {
        m_particle_spawners.UpdateItems(engine);
    }
}

void ParticleSystem::Render(Engine *engine, Frame *frame)
{
    AssertReady();

    const auto frame_index = frame->GetFrameIndex();
    const auto scene_id = engine->render_state.GetScene().id;
    const auto scene_index = scene_id ? scene_id.value - 1 : 0u;

    FixedArray<UInt, num_async_rendering_command_buffers> command_buffers_recorded_states { };
    
    // always run renderer items as HIGH priority,
    // so we do not lock up because we're waiting for a large process to
    // complete in the same thread
    engine->task_system.ParallelForEach(
        TaskPriority::HIGH,
        m_particle_spawners.GetItems(),
        [this, &command_buffers_recorded_states, frame_index, scene_index](const Handle<ParticleSpawner> &particle_spawner, UInt index, UInt batch_index) {
            auto *engine = GetEngine();
            auto *pipeline = particle_spawner->GetRendererInstance()->GetPipeline();

            m_command_buffers[frame_index][batch_index]->Record(
                engine->GetDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary) {
                    pipeline->Bind(secondary);

                    secondary->BindDescriptorSet(
                        engine->GetInstance()->GetDescriptorPool(),
                        pipeline,
                        &particle_spawner->GetDescriptorSets()[frame_index],
                        0,
                        FixedArray { static_cast<UInt32>(scene_index * sizeof(SceneShaderData)) }
                    );

                    m_quad_mesh->RenderIndirect(
                        engine,
                        secondary,
                        particle_spawner->GetIndirectBuffer().Get()
                    );

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[batch_index] = 1u;
        }
    );

    const auto num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (UInt i = 0; i < num_recorded_command_buffers; i++) {
        m_command_buffers[frame_index][i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }
}

} // namespace hyperion::v2
