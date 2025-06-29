/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <scene/Texture.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <Engine.hpp>

namespace hyperion {

enum RTRadianceUpdates : uint32
{
    RT_RADIANCE_UPDATES_NONE = 0x0,
    RT_RADIANCE_UPDATES_TLAS = 0x1,
    RT_RADIANCE_UPDATES_SHADOW_MAP = 0x2
};

#pragma region Render commands

struct RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)
    : RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight> image_views;

    RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)(
        const FixedArray<ImageViewRef, max_frames_in_flight>& image_views)
        : image_views(image_views)
    {
    }

    virtual ~RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)
    : RenderCommand
{
    virtual ~RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        // remove result image from global descriptor set
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("RTRadianceResultTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        return result;
    }
};

#pragma endregion Render commands

RaytracingReflections::RaytracingReflections(RaytracingReflectionsConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_updates { RT_RADIANCE_UPDATES_NONE, RT_RADIANCE_UPDATES_NONE }
{
}

RaytracingReflections::~RaytracingReflections()
{
}

void RaytracingReflections::Create()
{
    CreateImages();
    CreateUniformBuffer();
    CreateTemporalBlending();
    CreateRaytracingPipeline();
}

void RaytracingReflections::Destroy()
{
    m_shader.Reset();

    SafeRelease(std::move(m_raytracing_pipeline));

    SafeRelease(std::move(m_uniform_buffers));

    // remove result image from global descriptor set
    g_safe_deleter->SafeRelease(std::move(m_texture));

    PUSH_RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet);

    HYP_SYNC_RENDER();
}

void RaytracingReflections::UpdateUniforms(FrameBase* frame, const RenderSetup& render_setup)
{
    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(render_setup.view->GetView());

    RTRadianceUniforms uniforms {};

    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.min_roughness = 0.4f;
    uniforms.output_image_resolution = Vec2i(m_config.extent);

    uint32 num_bound_lights = 0;

    const uint32 max_bound_lights = ArraySize(uniforms.light_indices);

    for (Light* light : rpl.lights)
    {
        const LightType light_type = light->GetLightType();

        if (light_type != LT_DIRECTIONAL && light_type != LT_POINT)
        {
            continue;
        }

        if (num_bound_lights >= max_bound_lights)
        {
            break;
        }

        uniforms.light_indices[num_bound_lights++] = light->GetRenderResource().GetBufferIndex();
    }

    uniforms.num_bound_lights = num_bound_lights;

    m_uniform_buffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);

    if (m_updates[frame->GetFrameIndex()])
    {
        const DescriptorSetRef& descriptor_set = m_raytracing_pipeline->GetDescriptorTable()
                                                     ->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame->GetFrameIndex());

        AssertThrow(descriptor_set.IsValid());

        descriptor_set->Update();

        m_updates[frame->GetFrameIndex()] = RT_RADIANCE_UPDATES_NONE;
    }
}

void RaytracingReflections::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    UpdateUniforms(frame, render_setup);

    frame->GetCommandList().Add<BindRaytracingPipeline>(m_raytracing_pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_raytracing_pipeline->GetDescriptorTable(),
        m_raytracing_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(render_setup.env_grid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<InsertBarrier>(m_texture->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

    const Vec3u image_extent = m_texture->GetRenderResource().GetImage()->GetExtent();

    const SizeType num_pixels = image_extent.Volume();
    // const SizeType half_num_pixels = num_pixels / 2;

    frame->GetCommandList().Add<TraceRays>(
        m_raytracing_pipeline,
        Vec3u { uint32(num_pixels), 1, 1 });

    frame->GetCommandList().Add<InsertBarrier>(
        m_texture->GetRenderResource().GetImage(),
        RS_SHADER_RESOURCE);

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer() && render_setup.view->GetCamera()->GetBufferData().view != m_previous_view_matrix)
    {
        m_temporal_blending->ResetProgressiveBlending();

        m_previous_view_matrix = render_setup.view->GetCamera()->GetBufferData().view;
    }

    m_temporal_blending->Render(frame, render_setup);
}

void RaytracingReflections::CreateImages()
{
    m_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u { m_config.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE });

    InitObject(m_texture);

    m_texture->SetPersistentRenderResourceEnabled(true);
}

void RaytracingReflections::CreateUniformBuffer()
{
    RTRadianceUniforms uniforms;
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    m_uniform_buffers = {
        g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms)),
        g_render_backend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms))
    };

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        HYPERION_ASSERT_RESULT(m_uniform_buffers[frame_index]->Create());
        m_uniform_buffers[frame_index]->Copy(sizeof(uniforms), &uniforms);
    }
}

void RaytracingReflections::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags)
    {
        return;
    }

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = m_raytracing_pipeline->GetDescriptorTable()
                                                     ->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);

        AssertThrow(descriptor_set != nullptr);

        if (flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE)
        {
            // update acceleration structure in descriptor set
            descriptor_set->SetElement(NAME("TLAS"), m_top_level_acceleration_structures[frame_index]);
        }

        if (flags & RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS)
        {
            // update mesh descriptions buffer in descriptor set
            descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_top_level_acceleration_structures[frame_index]->GetMeshDescriptionsBuffer());
        }

        m_updates[frame_index] |= RT_RADIANCE_UPDATES_TLAS;
    }
}

void RaytracingReflections::CreateRaytracingPipeline()
{
    if (IsPathTracer())
    {
        m_shader = g_shader_manager->GetOrCreate(NAME("PathTracer"));
    }
    else
    {
        m_shader = g_shader_manager->GetOrCreate(NAME("RTRadiance"));
    }

    AssertThrow(m_shader.IsValid());

    const DescriptorTableDeclaration& descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        AssertThrow(m_top_level_acceleration_structures[frame_index] != nullptr);

        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("TLAS"), m_top_level_acceleration_structures[frame_index]);
        descriptor_set->SetElement(NAME("MeshDescriptionsBuffer"), m_top_level_acceleration_structures[frame_index]->GetMeshDescriptionsBuffer());

        descriptor_set->SetElement(NAME("OutputImage"), m_texture->GetRenderResource().GetImageView());

        descriptor_set->SetElement(NAME("LightsBuffer"), g_render_global_state->gpu_buffers[GRB_LIGHTS]->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("MaterialsBuffer"), g_render_global_state->gpu_buffers[GRB_MATERIALS]->GetBuffer(frame_index));
        descriptor_set->SetElement(NAME("RTRadianceUniforms"), m_uniform_buffers[frame_index]);
    }

    DeferCreate(descriptor_table);

    m_raytracing_pipeline = g_render_backend->MakeRaytracingPipeline(
        m_shader,
        descriptor_table);

    DeferCreate(m_raytracing_pipeline);

    PUSH_RENDER_COMMAND(
        SetRTRadianceImageInGlobalDescriptorSet,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending->GetResultTexture()->GetRenderResource().GetImageView(),
            m_temporal_blending->GetResultTexture()->GetRenderResource().GetImageView() });
}

void RaytracingReflections::CreateTemporalBlending()
{
    m_temporal_blending = MakeUnique<TemporalBlending>(
        m_config.extent,
        TF_RGBA16F,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        IsPathTracer()
            ? TemporalBlendFeedback::HIGH
            : TemporalBlendFeedback::HIGH,
        m_texture->GetRenderResource().GetImageView(),
        m_gbuffer);

    m_temporal_blending->Create();
}

} // namespace hyperion