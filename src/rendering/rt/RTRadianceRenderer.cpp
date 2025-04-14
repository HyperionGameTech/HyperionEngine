/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/rt/DDGI.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <scene/Texture.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::ResourceState;

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

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("RTRadianceResultTexture"), image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet) : renderer::RenderCommand
{
    virtual ~RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        // remove result image from global descriptor set
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("RTRadianceResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
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

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const GPUBufferRef &uniform_buffer = uniform_buffers[frame_index];

            HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms)));
            uniform_buffer->Memset(g_engine->GetGPUDevice(), sizeof(RTRadianceUniforms), 0x0);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

RTRadianceRenderer::RTRadianceRenderer(const Vec2u &extent, RTRadianceRendererOptions options)
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
    CreateImages();
    CreateUniformBuffer();
    CreateTemporalBlending();
    CreateRaytracingPipeline();
}

void RTRadianceRenderer::Destroy()
{
    m_shader.Reset();

    SafeRelease(std::move(m_raytracing_pipeline));

    SafeRelease(std::move(m_uniform_buffers));
    
    // remove result image from global descriptor set
    g_safe_deleter->SafeRelease(std::move(m_texture));

    PUSH_RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet);

    HYP_SYNC_RENDER();
}

void RTRadianceRenderer::UpdateUniforms(Frame *frame)
{
    RTRadianceUniforms uniforms { };

    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.min_roughness = 0.4f;

    uniforms.output_image_resolution = Vec2i(m_extent);

    const uint32 max_bound_lights = MathUtil::Min(g_engine->GetRenderState()->NumBoundLights(), ArraySize(uniforms.light_indices));
    uint32 num_bound_lights = 0;

    for (uint32 light_type = 0; light_type < uint32(LightType::MAX); light_type++) {
        if (num_bound_lights >= max_bound_lights) {
            break;
        }

        for (const auto &it : g_engine->GetRenderState()->bound_lights[light_type]) {
            if (num_bound_lights >= max_bound_lights) {
                break;
            }

            uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
        }
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);

    if (m_updates[frame->GetFrameIndex()]) {
        const DescriptorSetRef &descriptor_set = m_raytracing_pipeline->GetDescriptorTable()
            ->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex());

        AssertThrow(descriptor_set.IsValid());

        HYPERION_ASSERT_RESULT(descriptor_set->Update(g_engine->GetGPUDevice()));

        m_updates[frame->GetFrameIndex()] = RT_RADIANCE_UPDATES_NONE;
    }
}

void RTRadianceRenderer::Render(Frame *frame)
{
    UpdateUniforms(frame);

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();
    const TResourceHandle<EnvProbeRenderResource> &env_probe_resource_handle = g_engine->GetRenderState()->GetActiveEnvProbe();
    EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    frame->GetCommandList().Add<BindRaytracingPipeline>(m_raytracing_pipeline);
    
    frame->GetCommandList().Add<BindDescriptorTable>(
        m_raytracing_pipeline->GetDescriptorTable(),
        m_raytracing_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid ? env_grid->GetComponentIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_resource_handle.Get(), 0) }
                }
            }
        },
        frame->GetFrameIndex()
    );

    frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), ResourceState::UNORDERED_ACCESS);

    const Vec3u image_extent = m_texture->GetRenderResource().GetImage()->GetExtent();

    const SizeType num_pixels = image_extent.Volume();
    //const SizeType half_num_pixels = num_pixels / 2;

    frame->GetCommandList().Add<TraceRays>(
        m_raytracing_pipeline,
        Vec3u { uint32(num_pixels), 1, 1 }
    );

    frame->GetCommandList().Add<InsertBarrier>(
        m_texture->GetRenderResource().GetImage(),
        ResourceState::SHADER_RESOURCE
    );

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer() && camera_resource_handle->GetBufferData().view != m_previous_view_matrix) {
        m_temporal_blending->ResetProgressiveBlending();

        m_previous_view_matrix = camera_resource_handle->GetBufferData().view;
    }

    m_temporal_blending->Render(frame);
}

void RTRadianceRenderer::CreateImages()
{
    m_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        InternalFormat::RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    m_texture->SetIsRWTexture(true);
    InitObject(m_texture);

    m_texture->SetPersistentRenderResourceEnabled(true);
}

void RTRadianceRenderer::CreateUniformBuffer()
{
    m_uniform_buffers = {
        MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER),
        MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER)
    };

    PUSH_RENDER_COMMAND(CreateRTRadianceUniformBuffers, m_uniform_buffers);
}

void RTRadianceRenderer::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = m_raytracing_pipeline->GetDescriptorTable()
            ->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);

        AssertThrow(descriptor_set != nullptr);

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            descriptor_set->SetElement(NAME("TLAS"), m_top_level_acceleration_structures[frame_index]);
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_top_level_acceleration_structures[frame_index]->GetMeshDescriptionsBuffer());
        }

        m_updates[frame_index] |= RT_RADIANCE_UPDATES_TLAS;
    }
}

void RTRadianceRenderer::CreateRaytracingPipeline()
{
    if (IsPathTracer()) {
        m_shader = g_shader_manager->GetOrCreate(NAME("PathTracer"));
    } else {
        m_shader = g_shader_manager->GetOrCreate(NAME("RTRadiance"));
    }

    AssertThrow(m_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        AssertThrow(m_top_level_acceleration_structures[frame_index] != nullptr);

        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), m_top_level_acceleration_structures[frame_index]);
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_top_level_acceleration_structures[frame_index]->GetMeshDescriptionsBuffer());

        descriptor_set->SetElement(NAME("OutputImage"), m_texture->GetRenderResource().GetImageView());
        
        descriptor_set->SetElement(NAME("LightsBuffer"), g_engine->GetRenderData()->lights->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_engine->GetRenderData()->materials->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("RTRadianceUniforms"), m_uniform_buffers[frame_index]);
    }

    DeferCreate(
        descriptor_table,
        g_engine->GetGPUDevice()
    );

    m_raytracing_pipeline = MakeRenderObject<RaytracingPipeline>(
        m_shader,
        descriptor_table
    );

    DeferCreate(
        m_raytracing_pipeline,
        g_engine->GetGPUDevice()
    );

    PUSH_RENDER_COMMAND(
        SetRTRadianceImageInGlobalDescriptorSet,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending->GetResultTexture()->GetRenderResource().GetImageView(),
            m_temporal_blending->GetResultTexture()->GetRenderResource().GetImageView()
        }
    );
}

void RTRadianceRenderer::CreateTemporalBlending()
{
    m_temporal_blending = MakeUnique<TemporalBlending>(
        m_extent,
        InternalFormat::RGBA16F,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        IsPathTracer()
            ? TemporalBlendFeedback::HIGH
            : TemporalBlendFeedback::HIGH,
        m_texture->GetRenderResource().GetImageView()
    );

    m_temporal_blending->Create();
}

} // namespace hyperion