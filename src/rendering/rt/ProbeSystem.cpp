#include <rendering/rt/ProbeSystem.hpp>
#include <Engine.hpp>
#include <Types.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::StorageImageDescriptor;
using renderer::StorageBufferDescriptor;
using renderer::UniformBufferDescriptor;
using renderer::TlasDescriptor;
using renderer::ResourceState;
using renderer::Result;

struct RENDER_COMMAND(CreateProbeGridImage) : RenderCommandBase2
{
    renderer::StorageImage *storage_image;

    RENDER_COMMAND(CreateProbeGridImage)(renderer::StorageImage *storage_image)
        : storage_image(storage_image)
    {
    }

    virtual Result operator()()
    {
        return storage_image->Create(Engine::Get()->GetDevice());
    }
};

struct RENDER_COMMAND(CreateProbeGridImageView) : RenderCommandBase2
{
    renderer::ImageView *image_view;
    renderer::Image *image;

    RENDER_COMMAND(CreateProbeGridImageView)(renderer::ImageView *image_view, renderer::Image *image)
        : image_view(image_view),
          image(image)
    {
    }

    virtual Result operator()()
    {
        return image_view->Create(Engine::Get()->GetDevice(), image);
    }
};

struct RENDER_COMMAND(CreateProbeGridDescriptors) : RenderCommandBase2
{
    UniquePtr<renderer::DescriptorSet> *descriptor_sets;
    renderer::UniformBuffer *uniform_buffer;
    renderer::ImageView *irradiance_image_view;
    renderer::ImageView *depth_image_view;

    RENDER_COMMAND(CreateProbeGridDescriptors)(
        UniquePtr<renderer::DescriptorSet> *descriptor_sets,
        renderer::UniformBuffer *uniform_buffer,
        renderer::ImageView *irradiance_image_view,
        renderer::ImageView *depth_image_view
    ) : descriptor_sets(descriptor_sets),
        uniform_buffer(uniform_buffer),
        irradiance_image_view(irradiance_image_view),
        depth_image_view(depth_image_view)
    {
    }

    virtual Result operator()()
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                Engine::Get()->GetDevice(),
                &Engine::Get()->GetInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = Engine::Get()->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<UniformBufferDescriptor>(DescriptorKey::RT_PROBE_UNIFORMS)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = uniform_buffer
                });

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_IRRADIANCE_GRID)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = irradiance_image_view
                });

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_DEPTH_GRID)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = depth_image_view
                });
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyProbeGridDescriptors) : RenderCommandBase2
{
    RENDER_COMMAND(DestroyProbeGridDescriptors)()
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        Device *device = Engine::Get()->GetDevice();
        
        // remove result image from global descriptor set
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set_globals = Engine::Get()->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            // set to placeholder image
            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_IRRADIANCE_GRID)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8()
                });
            
            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_DEPTH_GRID)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateProbeGridUniformBuffer) : RenderCommandBase2
{
    renderer::UniformBuffer *uniform_buffer;
    ProbeSystemUniforms uniforms;

    RENDER_COMMAND(CreateProbeGridUniformBuffer)(renderer::UniformBuffer *uniform_buffer, ProbeSystemUniforms &&uniforms)
        : uniform_buffer(uniform_buffer),
          uniforms(std::move(uniforms))
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(Engine::Get()->GetDevice(), sizeof(ProbeSystemUniforms)));
        uniform_buffer->Copy(Engine::Get()->GetDevice(), sizeof(ProbeSystemUniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateProbeGridRadianceBuffer) : RenderCommandBase2
{
    renderer::StorageBuffer *radiance_buffer;
    ProbeGridInfo grid_info;

    RENDER_COMMAND(CreateProbeGridRadianceBuffer)(renderer::StorageBuffer *radiance_buffer, const ProbeGridInfo &grid_info)
        : radiance_buffer(radiance_buffer),
          grid_info(grid_info)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(radiance_buffer->Create(Engine::Get()->GetDevice(), grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData)));
        radiance_buffer->Memset(Engine::Get()->GetDevice(), grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData), 0x00);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateProbeGridPipeline) : RenderCommandBase2
{
    renderer::RaytracingPipeline *pipeline;
    renderer::ShaderProgram *shader_program;

    RENDER_COMMAND(CreateProbeGridPipeline)(renderer::RaytracingPipeline *pipeline, renderer::ShaderProgram *shader_program)
        : pipeline(pipeline),
          shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Create(
            Engine::Get()->GetDevice(),
            shader_program,
            &Engine::Get()->GetInstance()->GetDescriptorPool()
        );
    }
};

ProbeGrid::ProbeGrid(ProbeGridInfo &&grid_info)
    : m_grid_info(std::move(grid_info)),
      m_has_tlas_updates { false, false },
      m_time(0)
{
}

ProbeGrid::~ProbeGrid()
{
}

void ProbeGrid::Init()
{
    AssertThrowMsg(
        Engine::Get()->InitObject(m_tlas),
        "Failed to initialize the top level acceleration structure!"
    );

    DebugLog(
        LogType::Info,
        "Creating %u DDGI probes\n",
        m_grid_info.NumProbes()
    );

    const auto grid = m_grid_info.NumProbesPerDimension();
    m_probes.Resize(m_grid_info.NumProbes());

    for (UInt32 x = 0; x < grid.width; x++) {
        for (UInt32 y = 0; y < grid.height; y++) {
            for (UInt32 z = 0; z < grid.depth; z++) {
                const UInt32 index = x * grid.height * grid.depth
                     + y * grid.depth
                     + z;

                m_probes[index] = Probe {
                    .position = (Vector3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) - (Vector3(m_grid_info.probe_border) * 0.5f)) * m_grid_info.probe_distance
                };
            }
        }
    }

    CreateStorageBuffers();
    CreateUniformBuffer();
    CreateDescriptorSets();

    /* TMP */
    Engine::Get()->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this](...) {
        CreatePipeline();
    });
    
    CreateComputePipelines();
}

void ProbeGrid::Destroy()
{
    Engine::Get()->SafeReleaseHandle(std::move(m_shader));

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        Engine::Get()->SafeRelease(std::move(descriptor_set));
    }

    Engine::Get()->SafeRelease(std::move(m_uniform_buffer));
    Engine::Get()->SafeRelease(std::move(m_radiance_buffer));
    Engine::Get()->SafeRelease(std::move(m_irradiance_image));
    Engine::Get()->SafeRelease(std::move(m_irradiance_image_view));
    Engine::Get()->SafeRelease(std::move(m_depth_image));
    Engine::Get()->SafeRelease(std::move(m_depth_image_view));
    Engine::Get()->SafeRelease(std::move(m_pipeline));

    RenderCommands::Push<RENDER_COMMAND(DestroyProbeGridDescriptors)>();

    HYP_FLUSH_RENDER_QUEUE();
}

void ProbeGrid::CreatePipeline()
{
    m_shader = Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("RTProbe"));
    Engine::Get()->InitObject(m_shader);

    m_pipeline.Reset(new RaytracingPipeline(
        Array<const DescriptorSet *> {
            m_descriptor_sets[0].Get(),
            Engine::Get()->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
            Engine::Get()->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        }
    ));

    RenderCommands::Push<RENDER_COMMAND(CreateProbeGridPipeline)>(
        m_pipeline.Get(),
        m_shader->GetShaderProgram()
    );
}

void ProbeGrid::CreateComputePipelines()
{
    m_update_irradiance = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("RTProbeUpdateIrradiance")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    Engine::Get()->InitObject(m_update_irradiance);

    m_update_depth = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("RTProbeUpdateDepth")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    Engine::Get()->InitObject(m_update_depth);

    m_copy_border_texels_irradiance = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("RTCopyBorderTexelsIrradiance")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    Engine::Get()->InitObject(m_copy_border_texels_irradiance);

    m_copy_border_texels_depth = Engine::Get()->CreateHandle<ComputePipeline>(
        Engine::Get()->CreateHandle<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("RTCopyBorderTexelsDepth")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    Engine::Get()->InitObject(m_copy_border_texels_depth);
}

void ProbeGrid::CreateUniformBuffer()
{
    m_uniform_buffer.Reset(new UniformBuffer);

    RenderCommands::Push<RENDER_COMMAND(CreateProbeGridUniformBuffer)>(
        m_uniform_buffer.Get(),
        ProbeSystemUniforms {
            .aabb_max = Vector4(m_grid_info.aabb.max, 1.0f),
            .aabb_min = Vector4(m_grid_info.aabb.min, 1.0f),
            .probe_border = Vector4(m_grid_info.probe_border, 0.0f),
            .probe_counts = Vector4(m_grid_info.NumProbesPerDimension(), 0.0f),
            .grid_dimensions = Vector4(m_grid_info.GetImageDimensions(), 0.0f, 0.0f),
            .image_dimensions = Vector4(Extent2D(m_irradiance_image->GetExtent()), Extent2D(m_depth_image->GetExtent())),
            .params = Vector4(m_grid_info.probe_distance, static_cast<Float>(m_grid_info.num_rays_per_probe), 0.0f, 0.0f)
        }
    );
}

void ProbeGrid::CreateStorageBuffers()
{
    const Extent3D probe_counts = m_grid_info.NumProbesPerDimension();

    m_radiance_buffer.Reset(new StorageBuffer);

    RenderCommands::Push<RENDER_COMMAND(CreateProbeGridRadianceBuffer)>(
        m_radiance_buffer.Get(),
        m_grid_info
    );

    { // irradiance image
        constexpr InternalFormat irradiance_format = InternalFormat::RGBA16F;

        const Extent3D extent {
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2,
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.depth + 2,
            1
        };

        void *zeros = Memory::AllocateZeros(extent.Size() * static_cast<SizeType>(NumComponents(irradiance_format)));

        m_irradiance_image.Reset(new StorageImage(
            extent,
            irradiance_format,
            ImageType::TEXTURE_TYPE_2D,
            static_cast<UByte *>(zeros)
        ));

        Memory::DestructAndFree<void>(zeros);

        RenderCommands::Push<RENDER_COMMAND(CreateProbeGridImage)>(m_irradiance_image.Get());
    }

    { // irradiance image view
        m_irradiance_image_view.Reset(new ImageView);

        RenderCommands::Push<RENDER_COMMAND(CreateProbeGridImageView)>(m_irradiance_image_view.Get(), m_irradiance_image.Get());
    }

    { // depth image
        constexpr InternalFormat depth_format = InternalFormat::RG16F;

        const Extent3D extent {
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2,
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.depth + 2,
            1
        };

        void *zeros = Memory::AllocateZeros(extent.Size() * static_cast<SizeType>(NumComponents(depth_format)));

        m_depth_image.Reset(new StorageImage(
            extent,
            depth_format,
            ImageType::TEXTURE_TYPE_2D,
            static_cast<UByte *>(zeros)
        ));

        Memory::DestructAndFree<void>(zeros);

        RenderCommands::Push<RENDER_COMMAND(CreateProbeGridImage)>(m_depth_image.Get());
    }

    { // depth image view
        m_depth_image_view.Reset(new ImageView);

        RenderCommands::Push<RENDER_COMMAND(CreateProbeGridImageView)>(m_depth_image_view.Get(), m_depth_image.Get());
    }
}

void ProbeGrid::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        descriptor_set->GetOrAddDescriptor<TlasDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .acceleration_structure = &m_tlas->GetInternalTLAS()
            });

        // mesh descriptions
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer()
            });
        
        // materials
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(5)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = Engine::Get()->GetRenderData()->materials.GetBuffers()[frame_index].get()
            });
        
        // entities
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(6)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = Engine::Get()->GetRenderData()->objects.GetBuffers()[frame_index].get()
            });

        descriptor_set
            ->GetOrAddDescriptor<UniformBufferDescriptor>(9)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_uniform_buffer.Get()
            });

        descriptor_set
            ->GetOrAddDescriptor<StorageBufferDescriptor>(10)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_radiance_buffer.Get()
            });

        descriptor_set
            ->GetOrAddDescriptor<StorageImageDescriptor>(11)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_irradiance_image_view.Get()
            });

        descriptor_set
            ->GetOrAddDescriptor<StorageImageDescriptor>(12)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_depth_image_view.Get()
            });

        descriptor_set
            ->GetOrAddDescriptor<ImageDescriptor>(13)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_irradiance_image_view.Get()
            });

        descriptor_set
            ->GetOrAddDescriptor<ImageDescriptor>(14)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_depth_image_view.Get()
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    RenderCommands::Push<RENDER_COMMAND(CreateProbeGridDescriptors)>(
        m_descriptor_sets.Data(),
        m_uniform_buffer.Get(),
        m_irradiance_image_view.Get(),
        m_depth_image_view.Get()
    );
}

void ProbeGrid::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(0)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .acceleration_structure = &m_tlas->GetInternalTLAS()
                });
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(4)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer()
                });
        }

        m_has_tlas_updates[frame_index] = true;
    }
}

void ProbeGrid::SubmitPushConstants(CommandBuffer *command_buffer)
{
    m_random_generator.Next();

    std::memcpy(
        m_pipeline->push_constants.probe_data.matrix,
        m_random_generator.matrix.values,
        sizeof(m_pipeline->push_constants.probe_data.matrix)
    );

    m_pipeline->push_constants.probe_data.time = m_time++;

    m_pipeline->SubmitPushConstants(command_buffer);
}

void ProbeGrid::RenderProbes(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    if (m_has_tlas_updates[frame->GetFrameIndex()]) {
        m_descriptor_sets[frame->GetFrameIndex()]->ApplyUpdates(Engine::Get()->GetDevice());

        m_has_tlas_updates[frame->GetFrameIndex()] = false;
    }

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);
    
    SubmitPushConstants(frame->GetCommandBuffer());

    m_pipeline->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_pipeline.Get(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, frame->GetFrameIndex()),
        1,
        FixedArray {
            UInt32(sizeof(SceneShaderData) * Engine::Get()->render_state.GetScene().id.ToIndex()),
            UInt32(sizeof(LightShaderData) * 0)
        }
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
        2
    );

    m_pipeline->TraceRays(
        Engine::Get()->GetDevice(),
        frame->GetCommandBuffer(),
        Extent3D {
            m_grid_info.NumProbes(),
            m_grid_info.num_rays_per_probe,
            1u
        }
    );

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);
}

void ProbeGrid::ComputeIrradiance(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto probe_counts = m_grid_info.NumProbesPerDimension();

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );
    
    m_update_irradiance->GetPipeline()->Bind(frame->GetCommandBuffer());
    
    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_update_irradiance->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    m_update_irradiance->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            probe_counts.width * probe_counts.height,
            probe_counts.depth,
            1u
        }
    );

    m_update_depth->GetPipeline()->Bind(frame->GetCommandBuffer());
    
    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_update_depth->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    m_update_depth->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            probe_counts.width * probe_counts.height,
            probe_counts.depth,
            1u
        }
    );

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    // now copy border texels
    m_copy_border_texels_irradiance->GetPipeline()->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_copy_border_texels_irradiance->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    m_copy_border_texels_irradiance->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            probe_counts.width * probe_counts.height,
            probe_counts.depth,
            1u
        }
    );
    
    m_copy_border_texels_depth->GetPipeline()->Bind(frame->GetCommandBuffer());
    
    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetInstance()->GetDescriptorPool(),
        m_copy_border_texels_depth->GetPipeline(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );
    
    m_copy_border_texels_depth->GetPipeline()->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            probe_counts.width * probe_counts.height,
            probe_counts.depth,
            1u
        }
    );

    m_irradiance_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );
}

} // namespace hyperion::v2