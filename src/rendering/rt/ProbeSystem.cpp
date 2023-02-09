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

struct RENDER_COMMAND(CreateProbeGridImage) : RenderCommand
{
    ImageRef storage_image;

    RENDER_COMMAND(CreateProbeGridImage)(const ImageRef &storage_image)
        : storage_image(storage_image)
    {
    }

    virtual Result operator()()
    {
        return storage_image->Create(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(CreateProbeGridImageView) : RenderCommand
{
    ImageViewRef image_view;
    ImageRef image;

    RENDER_COMMAND(CreateProbeGridImageView)(const ImageViewRef &image_view, const ImageRef &image)
        : image_view(image_view),
          image(image)
    {
    }

    virtual Result operator()()
    {
        return image_view->Create(Engine::Get()->GetGPUDevice(), image);
    }
};

struct RENDER_COMMAND(CreateProbeGridDescriptors) : RenderCommand
{
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;
    GPUBufferRef uniform_buffer;
    ImageViewRef irradiance_image_view;
    ImageViewRef depth_image_view;

    RENDER_COMMAND(CreateProbeGridDescriptors)(
        const FixedArray<DescriptorSetRef, max_frames_in_flight> &descriptor_sets,
        const GPUBufferRef &uniform_buffer,
        const ImageViewRef &irradiance_image_view,
        const ImageViewRef &depth_image_view
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
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<UniformBufferDescriptor>(DescriptorKey::RT_PROBE_UNIFORMS)
                ->SetElementBuffer(0, uniform_buffer);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_IRRADIANCE_GRID)
                ->SetElementSRV(0, irradiance_image_view);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_DEPTH_GRID)
                ->SetElementSRV(0, depth_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyProbeGridDescriptors) : RenderCommand
{
    RENDER_COMMAND(DestroyProbeGridDescriptors)()
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        Device *device = Engine::Get()->GetGPUDevice();
        
        // remove result image from global descriptor set
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
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

struct RENDER_COMMAND(CreateProbeGridUniformBuffer) : RenderCommand
{
    GPUBufferRef uniform_buffer;
    ProbeSystemUniforms uniforms;

    RENDER_COMMAND(CreateProbeGridUniformBuffer)(const GPUBufferRef &uniform_buffer, ProbeSystemUniforms &&uniforms)
        : uniform_buffer(uniform_buffer),
          uniforms(std::move(uniforms))
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(Engine::Get()->GetGPUDevice(), sizeof(ProbeSystemUniforms)));
        uniform_buffer->Copy(Engine::Get()->GetGPUDevice(), sizeof(ProbeSystemUniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateProbeGridRadianceBuffer) : RenderCommand
{
    GPUBufferRef radiance_buffer;
    ProbeGridInfo grid_info;

    RENDER_COMMAND(CreateProbeGridRadianceBuffer)(const GPUBufferRef &radiance_buffer, const ProbeGridInfo &grid_info)
        : radiance_buffer(radiance_buffer),
          grid_info(grid_info)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(radiance_buffer->Create(Engine::Get()->GetGPUDevice(), grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData)));
        radiance_buffer->Memset(Engine::Get()->GetGPUDevice(), grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData), 0x00);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateProbeGridPipeline) : RenderCommand
{
    RaytracingPipelineRef pipeline;
    ShaderProgramRef shader_program;

    RENDER_COMMAND(CreateProbeGridPipeline)(const RaytracingPipelineRef &pipeline, const ShaderProgramRef &shader_program)
        : pipeline(pipeline),
          shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Create(
            Engine::Get()->GetGPUDevice(),
            shader_program.Get(),
            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
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
        InitObject(m_tlas),
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
                const UInt32 index = x * grid.height * grid.depth + y * grid.depth + z;

                m_probes[index] = Probe {
                    (Vector3(Float(x), Float(y), Float(z)) - (Vector3(m_grid_info.probe_border) * 0.5f)) * m_grid_info.probe_distance
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
        SafeRelease(std::move(descriptor_set));
    }

    SafeRelease(std::move(m_uniform_buffer));
    SafeRelease(std::move(m_radiance_buffer));
    SafeRelease(std::move(m_irradiance_image));
    SafeRelease(std::move(m_irradiance_image_view));
    SafeRelease(std::move(m_depth_image));
    SafeRelease(std::move(m_depth_image_view));
    SafeRelease(std::move(m_pipeline));

    PUSH_RENDER_COMMAND(DestroyProbeGridDescriptors);

    HYP_SYNC_RENDER();
}

void ProbeGrid::CreatePipeline()
{
    m_shader = Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(RTProbe));
    InitObject(m_shader);

    m_pipeline = RenderObjects::Make<RaytracingPipeline>(
        Array<const DescriptorSet *> {
            m_descriptor_sets[0].Get(),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
            Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        }
    );

    PUSH_RENDER_COMMAND(
        CreateProbeGridPipeline,
        m_pipeline,
        m_shader->GetShaderProgram()
    );
}

void ProbeGrid::CreateComputePipelines()
{
    m_update_irradiance = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(RTProbeUpdateIrradiance)),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_update_irradiance);

    m_update_depth = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(RTProbeUpdateDepth)),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_update_depth);

    m_copy_border_texels_irradiance = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(RTCopyBorderTexelsIrradiance)),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_copy_border_texels_irradiance);

    m_copy_border_texels_depth = CreateObject<ComputePipeline>(
        Engine::Get()->GetShaderManager().GetOrCreate(HYP_NAME(RTCopyBorderTexelsDepth)),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_copy_border_texels_depth);
}

void ProbeGrid::CreateUniformBuffer()
{
    m_uniform_buffer = RenderObjects::Make<GPUBuffer>(UniformBuffer());

    const Extent2D grid_image_dimensions = m_grid_info.GetImageDimensions();
    const Extent3D num_probes_per_dimension = m_grid_info.NumProbesPerDimension();

    PUSH_RENDER_COMMAND(
        CreateProbeGridUniformBuffer,
        m_uniform_buffer,
        ProbeSystemUniforms {
            .aabb_max = Vector4(m_grid_info.aabb.max, 1.0f),
            .aabb_min = Vector4(m_grid_info.aabb.min, 1.0f),
            .probe_border = {
                m_grid_info.probe_border.width,
                m_grid_info.probe_border.height,
                m_grid_info.probe_border.depth,
                0
            },
            .probe_counts = {
                num_probes_per_dimension.width,
                num_probes_per_dimension.height,
                num_probes_per_dimension.depth,
                0
            },
            .grid_dimensions = {
                grid_image_dimensions.width,
                grid_image_dimensions.height,
                0, 0
            },
            .image_dimensions = {
                m_irradiance_image->GetExtent().width,
                m_irradiance_image->GetExtent().height,
                m_depth_image->GetExtent().width,
                m_depth_image->GetExtent().height
            },
            .params = Vector4(m_grid_info.probe_distance, static_cast<Float>(m_grid_info.num_rays_per_probe), 0.0f, 0.0f)
        }
    );
}

void ProbeGrid::CreateStorageBuffers()
{
    const Extent3D probe_counts = m_grid_info.NumProbesPerDimension();

    m_radiance_buffer = RenderObjects::Make<GPUBuffer>(StorageBuffer());

    PUSH_RENDER_COMMAND(
        CreateProbeGridRadianceBuffer,
        m_radiance_buffer,
        m_grid_info
    );

    { // irradiance image
        constexpr InternalFormat irradiance_format = InternalFormat::RGBA16F;

        const Extent3D extent {
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2,
            (m_grid_info.irradiance_octahedron_size + 2) * probe_counts.depth + 2,
            1
        };

        void *zeros = Memory::AllocateZeros(extent.Size() * SizeType(NumComponents(irradiance_format)));

        m_irradiance_image = RenderObjects::Make<Image>(StorageImage(
            extent,
            irradiance_format,
            ImageType::TEXTURE_TYPE_2D,
            static_cast<UByte *>(zeros)
        ));

        Memory::DestructAndFree<void>(zeros);

        PUSH_RENDER_COMMAND(CreateProbeGridImage, m_irradiance_image);
    }

    { // irradiance image view
        m_irradiance_image_view = RenderObjects::Make<ImageView>();

        PUSH_RENDER_COMMAND(CreateProbeGridImageView, m_irradiance_image_view, m_irradiance_image);
    }

    { // depth image
        constexpr InternalFormat depth_format = InternalFormat::RG16F;

        const Extent3D extent {
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2,
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.depth + 2,
            1
        };

        void *zeros = Memory::AllocateZeros(extent.Size() * SizeType(NumComponents(depth_format)));

        m_depth_image = RenderObjects::Make<Image>(StorageImage(
            extent,
            depth_format,
            ImageType::TEXTURE_TYPE_2D,
            static_cast<UByte *>(zeros)
        ));

        Memory::DestructAndFree<void>(zeros);

        PUSH_RENDER_COMMAND(CreateProbeGridImage, m_depth_image);
    }

    { // depth image view
        m_depth_image_view = RenderObjects::Make<ImageView>();

        PUSH_RENDER_COMMAND(CreateProbeGridImageView, m_depth_image_view, m_depth_image);
    }
}

void ProbeGrid::CreateDescriptorSets()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = RenderObjects::Make<DescriptorSet>();

        descriptor_set->GetOrAddDescriptor<TlasDescriptor>(0)
            ->SetElementAccelerationStructure(0, &m_tlas->GetInternalTLAS());

        // mesh descriptions
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(4)
            ->SetElementBuffer(0, m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer());
        
        // materials
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(5)
            ->SetElementBuffer(0, Engine::Get()->GetRenderData()->materials.GetBuffers()[frame_index].get());
        
        // entities
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(6)
            ->SetElementBuffer(0, Engine::Get()->GetRenderData()->objects.GetBuffers()[frame_index].get());

        descriptor_set
            ->GetOrAddDescriptor<UniformBufferDescriptor>(9)
            ->SetElementBuffer(0, m_uniform_buffer.Get());

        descriptor_set
            ->GetOrAddDescriptor<StorageBufferDescriptor>(10)
            ->SetElementBuffer(0, m_radiance_buffer.Get());

        descriptor_set
            ->GetOrAddDescriptor<StorageImageDescriptor>(11)
            ->SetElementUAV(0, m_irradiance_image_view.Get());

        descriptor_set
            ->GetOrAddDescriptor<StorageImageDescriptor>(12)
            ->SetElementUAV(0, m_depth_image_view.Get());

        descriptor_set
            ->GetOrAddDescriptor<ImageDescriptor>(13)
            ->SetElementSRV(0, m_irradiance_image_view.Get());

        descriptor_set
            ->GetOrAddDescriptor<ImageDescriptor>(14)
            ->SetElementSRV(0, m_depth_image_view.Get());

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    PUSH_RENDER_COMMAND(
        CreateProbeGridDescriptors,
        m_descriptor_sets,
        m_uniform_buffer,
        m_irradiance_image_view,
        m_depth_image_view
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
                ->SetElementBuffer(0, m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer());
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
        m_descriptor_sets[frame->GetFrameIndex()]->ApplyUpdates(Engine::Get()->GetGPUDevice());

        m_has_tlas_updates[frame->GetFrameIndex()] = false;
    }

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);
    
    SubmitPushConstants(frame->GetCommandBuffer());

    m_pipeline->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_pipeline.Get(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, frame->GetFrameIndex()),
        1,
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->GetRenderState().GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Light, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0),
            HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
        }
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
        2
    );

    m_pipeline->TraceRays(
        Engine::Get()->GetGPUDevice(),
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
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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
        ResourceState::SHADER_RESOURCE
    );

    m_depth_image->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );
}

} // namespace hyperion::v2