/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/rt/DDGI.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ResourceState;
using renderer::Result;

enum RTRadianceUpdates : uint32
{
    RT_RADIANCE_UPDATES_NONE = 0x0,
    RT_RADIANCE_UPDATES_TLAS = 0x1,
    RT_RADIANCE_UPDATES_SHADOW_MAP = 0x2
};

#pragma region Render commands

struct RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet) : renderer::RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight>  image_views;

    RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)(
        const FixedArray<ImageViewRef, max_frames_in_flight> &image_views
    ) : image_views(image_views)
    {
    }

    virtual ~RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(RTRadianceResultTexture), image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet) : renderer::RenderCommand
{
    virtual ~RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        Result result;

        // remove result image from global descriptor set
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Global), frame_index)
                ->SetElement(HYP_NAME(RTRadianceResultTexture), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateRTRadianceUniformBuffers) : renderer::RenderCommand
{
    FixedArray<GPUBufferRef, max_frames_in_flight>  uniform_buffers;

    RENDER_COMMAND(CreateRTRadianceUniformBuffers)(FixedArray<GPUBufferRef, max_frames_in_flight> uniform_buffers)
        : uniform_buffers(std::move(uniform_buffers))
    {
    }

    virtual ~RENDER_COMMAND(CreateRTRadianceUniformBuffers)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            GPUBufferRef &uniform_buffer = uniform_buffers[frame_index];

            HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms)));
            uniform_buffer->Memset(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms), 0x0);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

RTRadianceRenderer::RTRadianceRenderer(const Extent2D &extent, RTRadianceRendererOptions options)
    : m_extent(extent),
      m_options(options),
      m_updates { RT_RADIANCE_UPDATES_NONE, RT_RADIANCE_UPDATES_NONE }
{
}

RTRadianceRenderer::~RTRadianceRenderer()
{
}

void RTRadianceRenderer::Create()
{
    AssertThrowMsg(
        InitObject(m_tlas),
        "Failed to initialize the top level acceleration structure!"
    );

    CreateImages();
    CreateUniformBuffer();
    CreateTemporalBlending();
    CreateRaytracingPipeline();
}

void RTRadianceRenderer::Destroy()
{
    m_temporal_blending->Destroy();

    m_shader.Reset();

    SafeRelease(std::move(m_raytracing_pipeline));

    SafeRelease(std::move(m_uniform_buffers));
    
    // remove result image from global descriptor set
    g_safe_deleter->SafeReleaseHandle(std::move(m_texture));

    PUSH_RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet);

    HYP_SYNC_RENDER();
}

void RTRadianceRenderer::UpdateUniforms(Frame *frame)
{
    RTRadianceUniforms uniforms { };

    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    const uint32 num_bound_lights = MathUtil::Min(uint32(g_engine->GetRenderState().lights.Size()), 16);

    for (uint32 index = 0; index < num_bound_lights; index++) {
        uniforms.light_indices[index] = (g_engine->GetRenderState().lights.Data() + index)->first.ToIndex();
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);

    if (m_updates[frame->GetFrameIndex()]) {
        m_raytracing_pipeline->GetDescriptorTable()->Update(g_engine->GetGPUDevice(), frame->GetFrameIndex());

        m_updates[frame->GetFrameIndex()] = RT_RADIANCE_UPDATES_NONE;
    }
}

void RTRadianceRenderer::Render(Frame *frame)
{
    UpdateUniforms(frame);

    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    m_raytracing_pipeline->GetDescriptorTable()->Bind(
        frame,
        m_raytracing_pipeline,
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

    m_texture->GetImage()->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    const Extent3D image_extent = m_texture->GetImage()->GetExtent();
    const SizeType num_pixels = image_extent.Size();
    
    const SizeType half_num_pixels = num_pixels / 2;

    m_raytracing_pipeline->TraceRays(
        g_engine->GetGPUDevice(),
        frame->GetCommandBuffer(),
        Extent3D {
            uint32(half_num_pixels), 1, 1
        }
    );

    m_texture->GetImage()->GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer() && g_engine->GetRenderState().GetCamera().camera.view != m_previous_view_matrix) {
        m_temporal_blending->ResetProgressiveBlending();

        m_previous_view_matrix = g_engine->GetRenderState().GetCamera().camera.view;
    }

    m_temporal_blending->Render(frame);
}

void RTRadianceRenderer::CreateImages()
{
    m_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        InternalFormat::RGBA8,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    m_texture->GetImage()->SetIsRWTexture(true);
    InitObject(m_texture);
}

void RTRadianceRenderer::CreateUniformBuffer()
{
    m_uniform_buffers = {
        MakeRenderObject<GPUBuffer>(UniformBuffer()),
        MakeRenderObject<GPUBuffer>(UniformBuffer())
    };

    PUSH_RENDER_COMMAND(CreateRTRadianceUniformBuffers, m_uniform_buffers);
}

void RTRadianceRenderer::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = m_raytracing_pipeline->GetDescriptorTable()
            ->GetDescriptorSet(HYP_NAME(RTRadianceDescriptorSet), frame_index);

        AssertThrow(descriptor_set != nullptr);

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            descriptor_set->SetElement(HYP_NAME(TLAS), m_tlas->GetInternalTLAS());
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            descriptor_set->SetElement(HYP_NAME(MeshDescriptionsBuffer), m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());
        }

        m_updates[frame_index] |= RT_RADIANCE_UPDATES_TLAS;
    }
}

void RTRadianceRenderer::CreateRaytracingPipeline()
{
    if (IsPathTracer()) {
        m_shader = g_shader_manager->GetOrCreate(HYP_NAME(PathTracer));
    } else {
        m_shader = g_shader_manager->GetOrCreate(HYP_NAME(RTRadiance));
    }

    if (!InitObject(m_shader)) {
        return;
    }

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(RTRadianceDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(TLAS), m_tlas->GetInternalTLAS());
        descriptor_set->SetElement(HYP_NAME(MeshDescriptionsBuffer), m_tlas->GetInternalTLAS()->GetMeshDescriptionsBuffer());

        descriptor_set->SetElement(HYP_NAME(OutputImage), m_texture->GetImageView());
        
        descriptor_set->SetElement(HYP_NAME(LightsBuffer), g_engine->GetRenderData()->lights.GetBuffer());
        descriptor_set->SetElement(HYP_NAME(MaterialsBuffer), g_engine->GetRenderData()->materials.GetBuffer());
        descriptor_set->SetElement(HYP_NAME(RTRadianceUniforms), m_uniform_buffers[frame_index]);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        m_shader->GetShaderProgram(),
        descriptor_table
    );

    DeferCreate(
        m_raytracing_pipeline,
        g_engine->GetGPUDevice()
    );

    PUSH_RENDER_COMMAND(
        SetRTRadianceImageInGlobalDescriptorSet,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending->GetImageOutput(0).image_view,
            m_temporal_blending->GetImageOutput(1).image_view
        }
    );
}

void RTRadianceRenderer::CreateTemporalBlending()
{
    m_temporal_blending.Reset(new TemporalBlending(
        m_extent,
        InternalFormat::RGBA8,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        IsPathTracer()
            ? TemporalBlendFeedback::HIGH
            : TemporalBlendFeedback::LOW,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_texture->GetImageView(),
            m_texture->GetImageView(),
        }
    ));

    m_temporal_blending->Create();
}

} // namespace hyperion::v2