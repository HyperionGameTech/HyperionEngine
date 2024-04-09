#include <rendering/rt/DDGI.hpp>
#include <Engine.hpp>
#include <Types.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/ByteUtil.hpp>

namespace hyperion::v2 {

using renderer::ResourceState;
using renderer::Result;

enum ProbeSystemUpdates : uint32
{
    PROBE_SYSTEM_UPDATES_NONE = 0x0,
    PROBE_SYSTEM_UPDATES_TLAS = 0x1
};

#pragma region Render commands

struct RENDER_COMMAND(CreateDDGIImage) : renderer::RenderCommand
{
    ImageRef storage_image;

    RENDER_COMMAND(CreateDDGIImage)(const ImageRef &storage_image)
        : storage_image(storage_image)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIImage)() override = default;

    virtual Result operator()() override
    {
        return storage_image->Create(g_engine->GetGPUDevice());
    }
};

struct RENDER_COMMAND(CreateDDGIImageView) : renderer::RenderCommand
{
    ImageViewRef image_view;
    ImageRef image;

    RENDER_COMMAND(CreateDDGIImageView)(const ImageViewRef &image_view, const ImageRef &image)
        : image_view(image_view),
          image(image)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIImageView)() override = default;

    virtual Result operator()() override
    {
        return image_view->Create(g_engine->GetGPUDevice(), image);
    }
};

struct RENDER_COMMAND(SetDDGIDescriptors) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;
    ImageViewRef irradiance_image_view;
    ImageViewRef depth_image_view;

    RENDER_COMMAND(SetDDGIDescriptors)(
        const GPUBufferRef &uniform_buffer,
        const ImageViewRef &irradiance_image_view,
        const ImageViewRef &depth_image_view
    ) : uniform_buffer(uniform_buffer),
        irradiance_image_view(irradiance_image_view),
        depth_image_view(depth_image_view)
    {
    }

    virtual ~RENDER_COMMAND(SetDDGIDescriptors)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(DDGIUniforms), uniform_buffer);

            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(DDGIIrradianceTexture), irradiance_image_view);

            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(DDGIDepthTexture), depth_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetDDGIDescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(UnsetDDGIDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(UnsetDDGIDescriptors)() override = default;

    virtual Result operator()() override
    {   
        // remove result image from global descriptor set
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(DDGIIrradianceTexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(DDGIDepthTexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIUniformBuffer) : renderer::RenderCommand
{
    GPUBufferRef uniform_buffer;
    DDGIUniforms uniforms;

    RENDER_COMMAND(CreateDDGIUniformBuffer)(const GPUBufferRef &uniform_buffer, const DDGIUniforms &uniforms)
        : uniform_buffer(uniform_buffer),
          uniforms(uniforms)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIUniformBuffer)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(DDGIUniforms)));
        uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(DDGIUniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIRadianceBuffer) : renderer::RenderCommand
{
    GPUBufferRef radiance_buffer;
    DDGIInfo grid_info;

    RENDER_COMMAND(CreateDDGIRadianceBuffer)(const GPUBufferRef &radiance_buffer, const DDGIInfo &grid_info)
        : radiance_buffer(radiance_buffer),
          grid_info(grid_info)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIRadianceBuffer)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(radiance_buffer->Create(g_engine->GetGPUDevice(), grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData)));
        radiance_buffer->Memset(g_engine->GetGPUDevice(), grid_info.GetImageDimensions().Size() * sizeof(ProbeRayData), 0x00);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

DDGI::DDGI(DDGIInfo &&grid_info)
    : m_grid_info(std::move(grid_info)),
      m_updates { PROBE_SYSTEM_UPDATES_NONE, PROBE_SYSTEM_UPDATES_NONE },
      m_time(0)
{
}

DDGI::~DDGI()
{
}

void DDGI::Init()
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

    for (uint32 x = 0; x < grid.width; x++) {
        for (uint32 y = 0; y < grid.height; y++) {
            for (uint32 z = 0; z < grid.depth; z++) {
                const uint32 index = x * grid.height * grid.depth + y * grid.depth + z;

                m_probes[index] = Probe {
                    (Vector3(float(x), float(y), float(z)) - (Vector3(m_grid_info.probe_border) * 0.5f)) * m_grid_info.probe_distance
                };
            }
        }
    }

    CreateStorageBuffers();
    CreateUniformBuffer();
    CreatePipelines();

    PUSH_RENDER_COMMAND(
        SetDDGIDescriptors,
        m_uniform_buffer,
        m_irradiance_image_view,
        m_depth_image_view
    );
}

void DDGI::Destroy()
{
    m_shader.Reset();
    
    SafeRelease(std::move(m_uniform_buffer));
    SafeRelease(std::move(m_radiance_buffer));
    SafeRelease(std::move(m_irradiance_image));
    SafeRelease(std::move(m_irradiance_image_view));
    SafeRelease(std::move(m_depth_image));
    SafeRelease(std::move(m_depth_image_view));
    SafeRelease(std::move(m_pipeline));
    SafeRelease(std::move(m_update_irradiance));
    SafeRelease(std::move(m_update_depth));
    SafeRelease(std::move(m_copy_border_texels_irradiance));
    SafeRelease(std::move(m_copy_border_texels_depth));

    PUSH_RENDER_COMMAND(UnsetDDGIDescriptors);

    HYP_SYNC_RENDER();
}

void DDGI::CreatePipelines()
{
    m_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTProbe));
    InitObject(m_shader);

    const DescriptorTableDeclaration raytracing_pipeline_descriptor_table_decl = m_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef raytracing_pipeline_descriptor_table = MakeRenderObject<renderer::DescriptorTable>(raytracing_pipeline_descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = raytracing_pipeline_descriptor_table->GetDescriptorSet(HYP_NAME(DDGIDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(TLAS), m_tlas->GetInternalTLAS());
        
        descriptor_set->SetElement(HYP_NAME(LightsBuffer), g_engine->GetRenderData()->lights.GetBuffer());
        descriptor_set->SetElement(HYP_NAME(MaterialsBuffer), g_engine->GetRenderData()->materials.GetBuffer());
        descriptor_set->SetElement(HYP_NAME(MeshDescriptionsBuffer), m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());

        descriptor_set->SetElement(HYP_NAME(DDGIUniforms), m_uniform_buffer);
        descriptor_set->SetElement(HYP_NAME(ProbeRayData), m_radiance_buffer);
    }

    DeferCreate(raytracing_pipeline_descriptor_table, g_engine->GetGPUDevice());

    // Create raytracing pipeline

    m_pipeline = MakeRenderObject<RaytracingPipeline>(
        m_shader->GetShaderProgram(),
        raytracing_pipeline_descriptor_table
    );

    DeferCreate(m_pipeline, g_engine->GetGPUDevice());

    Handle<Shader> update_irradiance_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTProbeUpdateIrradiance));
    Handle<Shader> update_depth_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTProbeUpdateDepth));
    Handle<Shader> copy_border_texels_irradiance_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTCopyBorderTexelsIrradiance));
    Handle<Shader> copy_border_texels_depth_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTCopyBorderTexelsDepth));

    Pair<Handle<Shader>, ComputePipelineRef &> shaders[] = {
        { update_irradiance_shader, m_update_irradiance },
        { update_depth_shader, m_update_depth },
        { copy_border_texels_irradiance_shader, m_copy_border_texels_irradiance },
        { copy_border_texels_depth_shader, m_copy_border_texels_depth }
    };

    for (auto &it : shaders) {
        InitObject(it.first);
        
        const DescriptorTableDeclaration descriptor_table_decl = it.first->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSet2Ref &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(DDGIDescriptorSet), frame_index);
            AssertThrow(descriptor_set != nullptr);

            descriptor_set->SetElement(HYP_NAME(DDGIUniforms), m_uniform_buffer);
            descriptor_set->SetElement(HYP_NAME(ProbeRayData), m_radiance_buffer);

            descriptor_set->SetElement(HYP_NAME(OutputIrradianceImage), m_irradiance_image_view);
            descriptor_set->SetElement(HYP_NAME(OutputDepthImage), m_depth_image_view);
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        it.second = MakeRenderObject<renderer::ComputePipeline>(
            it.first->GetShaderProgram(),
            descriptor_table
        );

        DeferCreate(it.second, g_engine->GetGPUDevice());
    }
}

void DDGI::CreateUniformBuffer()
{
    m_uniform_buffer = MakeRenderObject<GPUBuffer>(UniformBuffer());

    const Extent2D grid_image_dimensions = m_grid_info.GetImageDimensions();
    const Extent3D num_probes_per_dimension = m_grid_info.NumProbesPerDimension();

    m_uniforms = DDGIUniforms {
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
        .params = {
            ByteUtil::PackFloat(m_grid_info.probe_distance),
            m_grid_info.num_rays_per_probe,
            PROBE_SYSTEM_FLAGS_FIRST_RUN,
            0
        }
    };

    PUSH_RENDER_COMMAND(
        CreateDDGIUniformBuffer,
        m_uniform_buffer,
        m_uniforms
    );
}

void DDGI::CreateStorageBuffers()
{
    const Extent3D probe_counts = m_grid_info.NumProbesPerDimension();

    m_radiance_buffer = MakeRenderObject<GPUBuffer>(StorageBuffer());

    PUSH_RENDER_COMMAND(
        CreateDDGIRadianceBuffer,
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

        m_irradiance_image = MakeRenderObject<Image>(StorageImage(
            extent,
            irradiance_format,
            ImageType::TEXTURE_TYPE_2D,
            UniquePtr<MemoryStreamedData>::Construct(ByteBuffer(extent.Size() * SizeType(NumComponents(irradiance_format))))
        ));

        PUSH_RENDER_COMMAND(CreateDDGIImage, m_irradiance_image);
    }

    { // irradiance image view
        m_irradiance_image_view = MakeRenderObject<ImageView>();

        PUSH_RENDER_COMMAND(CreateDDGIImageView, m_irradiance_image_view, m_irradiance_image);
    }

    { // depth image
        constexpr InternalFormat depth_format = InternalFormat::RG16F;

        const Extent3D extent {
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.width * probe_counts.height + 2,
            (m_grid_info.depth_octahedron_size + 2) * probe_counts.depth + 2,
            1
        };

        m_depth_image = MakeRenderObject<Image>(StorageImage(
            extent,
            depth_format,
            ImageType::TEXTURE_TYPE_2D,
            UniquePtr<MemoryStreamedData>::Construct(ByteBuffer(extent.Size() * SizeType(NumComponents(depth_format))))
        ));

        PUSH_RENDER_COMMAND(CreateDDGIImage, m_depth_image);
    }

    { // depth image view
        m_depth_image_view = MakeRenderObject<ImageView>();

        PUSH_RENDER_COMMAND(CreateDDGIImageView, m_depth_image_view, m_depth_image);
    }
}

void DDGI::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = m_pipeline->GetDescriptorTable()->GetDescriptorSet(HYP_NAME(DDGIDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            descriptor_set->SetElement(HYP_NAME(TLAS), m_tlas->GetInternalTLAS());
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            descriptor_set->SetElement(HYP_NAME(MeshDescriptionsBuffer), m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());
        }

        descriptor_set->Update(g_engine->GetGPUDevice());

        m_updates[frame_index] &= ~PROBE_SYSTEM_UPDATES_TLAS;
    }
}

void DDGI::SubmitPushConstants(CommandBuffer *command_buffer)
{
    m_random_generator.Next();

    Memory::MemCpy(
        m_pipeline->push_constants.probe_data.matrix,
        m_random_generator.matrix.values,
        sizeof(m_pipeline->push_constants.probe_data.matrix)
    );

    m_pipeline->push_constants.probe_data.time = m_time++;

    m_pipeline->SubmitPushConstants(command_buffer);
}

void DDGI::UpdateUniforms(Frame *frame)
{
    const uint camera_index = g_engine->GetRenderState().GetCamera().id.ToIndex();

    uint32 num_bound_lights = 0;

    for (const auto &it : g_engine->GetRenderState().lights) {
        if (num_bound_lights == std::size(m_uniforms.light_indices)) {
            break;
        }

        const ID<Light> light_id = it.first;
        const LightDrawProxy &light = it.second;

        if (light.visibility_bits & (1ull << SizeType(camera_index))) {
            m_uniforms.light_indices[num_bound_lights] = light_id.ToIndex();
            num_bound_lights++;
        }
    }

    m_uniforms.params[3] = num_bound_lights;

    m_uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(DDGIUniforms), &m_uniforms);

    m_uniforms.params[2] &= ~PROBE_SYSTEM_FLAGS_FIRST_RUN;
}

void DDGI::RenderProbes(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    UpdateUniforms(frame);

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);

    m_pipeline->Bind(frame->GetCommandBuffer());
    
    SubmitPushConstants(frame->GetCommandBuffer());

    m_pipeline->GetDescriptorTable()->Bind(
        frame,
        m_pipeline,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Extent3D {
            m_grid_info.NumProbes(),
            m_grid_info.num_rays_per_probe,
            1u
        }
    );

    m_radiance_buffer->InsertBarrier(frame->GetCommandBuffer(), ResourceState::UNORDERED_ACCESS);
}

void DDGI::ComputeIrradiance(Frame *frame)
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
    
    m_update_irradiance->Bind(frame->GetCommandBuffer());

    m_update_irradiance->GetDescriptorTable()->Bind(
        frame,
        m_update_irradiance,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_update_irradiance->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            probe_counts.width * probe_counts.height,
            probe_counts.depth,
            1u
        }
    );

    m_update_depth->Bind(frame->GetCommandBuffer());

    m_update_depth->GetDescriptorTable()->Bind(
        frame,
        m_update_depth,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_update_depth->Dispatch(
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
    m_copy_border_texels_irradiance->Bind(frame->GetCommandBuffer());

    m_copy_border_texels_irradiance->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels_irradiance,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    m_copy_border_texels_irradiance->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (probe_counts.width * probe_counts.height * (m_grid_info.irradiance_octahedron_size + m_grid_info.probe_border.width)) + 7 / 8,
            (probe_counts.depth * (m_grid_info.irradiance_octahedron_size + m_grid_info.probe_border.depth)) + 7 / 8,
            1u
        }
    );
    
    m_copy_border_texels_depth->Bind(frame->GetCommandBuffer());

    m_copy_border_texels_depth->GetDescriptorTable()->Bind(
        frame,
        m_copy_border_texels_depth,
        {
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );
    
    m_copy_border_texels_depth->Dispatch(
        frame->GetCommandBuffer(),
        Extent3D {
            (probe_counts.width * probe_counts.height * (m_grid_info.depth_octahedron_size + m_grid_info.probe_border.width)) + 15 / 16,
            (probe_counts.depth * (m_grid_info.depth_octahedron_size + m_grid_info.probe_border.depth)) + 15 / 16,
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