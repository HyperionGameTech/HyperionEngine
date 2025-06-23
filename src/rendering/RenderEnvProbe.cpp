/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/AsyncCompute.hpp>

#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr Vec2u sh_num_samples = { 16, 16 };
static constexpr Vec2u sh_num_tiles = { 16, 16 };
static constexpr uint32 sh_num_levels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(sh_num_samples.Max()) + 1));
static constexpr bool sh_parallel_reduce = false;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Matrix4, 6> view_matrices;

    for (uint32 i = 0; i < 6; i++)
    {
        view_matrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemap_directions[i].first,
            Texture::cubemap_directions[i].second);
    }

    return view_matrices;
}

#pragma region RenderEnvProbe

RenderEnvProbe::RenderEnvProbe(EnvProbe* env_probe)
    : m_env_probe(env_probe),
      m_buffer_data {},
      m_texture_slot(~0u)
{
    if (!m_env_probe->IsControlledByEnvGrid())
    {
        CreateShader();
    }
}

RenderEnvProbe::~RenderEnvProbe()
{
    m_render_view.Reset();
    m_shadow_map.Reset();

    SafeRelease(std::move(m_shader));
}

void RenderEnvProbe::SetPositionInGrid(const Vec4i& position_in_grid)
{
    HYP_SCOPE;

    Execute([this, position_in_grid]()
        {
            m_position_in_grid = position_in_grid;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::SetTextureSlot(uint32 texture_slot)
{
    HYP_SCOPE;

    Execute([this, texture_slot]()
        {
            HYP_LOG(Rendering, Debug, "Setting texture slot for EnvProbe {} (type: {}) to {}", m_env_probe->GetID(), m_env_probe->GetEnvProbeType(), texture_slot);

            if (m_texture_slot == texture_slot)
            {
                return;
            }

            m_texture_slot = texture_slot;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::SetBufferData(const EnvProbeShaderData& buffer_data)
{
    HYP_SCOPE;

    Execute([this, buffer_data]()
        {
            // TEMP hack: save previous texture_index and position_in_grid
            const Vec4i position_in_grid = m_buffer_data.position_in_grid;

            m_buffer_data = buffer_data;

            // restore previous texture_index and position_in_grid
            m_buffer_data.texture_index = m_texture_slot;
            m_buffer_data.position_in_grid = position_in_grid;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::SetViewResourceHandle(TResourceHandle<RenderView>&& render_view)
{
    HYP_SCOPE;

    Execute([this, render_view = std::move(render_view)]()
        {
            if (m_render_view == render_view)
            {
                return;
            }

            m_render_view = std::move(render_view);
        });
}

void RenderEnvProbe::SetShadowMap(TResourceHandle<RenderShadowMap>&& shadow_map)
{
    HYP_SCOPE;

    Execute([this, shadow_map = std::move(shadow_map)]()
        {
            if (m_shadow_map == shadow_map)
            {
                return;
            }

            m_shadow_map = std::move(shadow_map);
        });
}

void RenderEnvProbe::SetSphericalHarmonics(const EnvProbeSphericalHarmonics& spherical_harmonics)
{
    HYP_SCOPE;

    Execute([this, spherical_harmonics]()
        {
            m_spherical_harmonics = spherical_harmonics;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::CreateShader()
{
    if (m_env_probe->IsControlledByEnvGrid())
    {
        return;
    }

    if (m_env_probe->IsReflectionProbe())
    {
        m_shader = g_shader_manager->GetOrCreate(
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "WRITE_NORMALS", "WRITE_MOMENTS" }));
    }
    else if (m_env_probe->IsSkyProbe())
    {
        m_shader = g_shader_manager->GetOrCreate(
            NAME("RenderSky"),
            ShaderProperties(static_mesh_vertex_attributes));
    }
    else if (m_env_probe->IsShadowProbe())
    {
        m_shader = g_shader_manager->GetOrCreate(
            NAME("RenderToCubemap"),
            ShaderProperties(static_mesh_vertex_attributes, { "MODE_SHADOWS" }));
    }
    else
    {
        HYP_UNREACHABLE();
    }

    AssertThrow(m_shader.IsValid());
}

void RenderEnvProbe::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderEnvProbe::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderEnvProbe::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

GPUBufferHolderBase* RenderEnvProbe::GetGPUBufferHolder() const
{
    return g_render_global_state->EnvProbes;
}

void RenderEnvProbe::UpdateBufferData()
{
    HYP_SCOPE;

    const BoundingBox aabb = BoundingBox(m_buffer_data.aabb_min.GetXYZ(), m_buffer_data.aabb_max.GetXYZ());
    const Vec3f world_position = m_buffer_data.world_position.GetXYZ();

    const FixedArray<Matrix4, 6> view_matrices = CreateCubemapMatrices(aabb, world_position);

    EnvProbeShaderData* buffer_data = static_cast<EnvProbeShaderData*>(m_buffer_address);

    Memory::MemCpy(buffer_data, &m_buffer_data, sizeof(EnvProbeShaderData));
    Memory::MemCpy(buffer_data->face_view_matrices, view_matrices.Data(), sizeof(EnvProbeShaderData::face_view_matrices));
    Memory::MemCpy(buffer_data->sh.values, m_spherical_harmonics.values, sizeof(EnvProbeSphericalHarmonics::values));

    if (m_env_probe->IsShadowProbe())
    {
        AssertThrow(m_shadow_map);

        buffer_data->texture_index = m_shadow_map->GetAtlasElement().point_light_index;
    }
    else
    {
        buffer_data->texture_index = m_texture_slot;
    }

    buffer_data->position_in_grid = m_position_in_grid;

    GetGPUBufferHolder()->MarkDirty(m_buffer_index);
}

/// TEMPORARY: will be replaced by EnvProbeRenderer classes.
void RenderEnvProbe::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_env_probe->IsControlledByEnvGrid())
    {
        HYP_LOG(EnvProbe, Warning, "EnvProbe {} is controlled by an EnvGrid, but Render() is being called!", m_env_probe->GetID());

        return;
    }

    AssertDebug(m_buffer_index != ~0u);

    RenderProxyList& rpl = GetConsumerRenderProxyList(m_render_view->GetView());

    if (!m_env_probe->NeedsRender())
    {
        return;
    }

    HYP_LOG(EnvProbe, Debug, "Rendering EnvProbe {} (type: {})",
        m_env_probe->GetID(), m_env_probe->GetEnvProbeType());

    const uint32 frame_index = frame->GetFrameIndex();

    RenderSetup new_render_setup = render_setup;
    new_render_setup.view = m_render_view.Get();

    {
        new_render_setup.env_probe = this;

        RenderCollector::ExecuteDrawCalls(
            frame,
            new_render_setup,
            rpl,
            ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

        new_render_setup.env_probe = nullptr;
    }

    const ViewOutputTarget& output_target = m_env_probe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const ImageRef& framebuffer_image = framebuffer->GetAttachment(0)->GetImage();

    if (m_env_probe->IsSkyProbe() || m_env_probe->IsReflectionProbe())
    {
        return; // now handled by ReflectionProbeRenderer
    }
    else if (m_env_probe->IsShadowProbe())
    {
        AssertThrow(m_shadow_map);
        AssertThrow(m_shadow_map->GetAtlasElement().point_light_index != ~0u);

        HYP_LOG(EnvProbe, Debug, "Render shadow probe {} (pointlight index: {})",
            m_env_probe->GetID(), m_shadow_map->GetAtlasElement().point_light_index);

        const ImageViewRef& shadow_map_image_view = m_shadow_map->GetImageView();
        AssertThrow(shadow_map_image_view.IsValid());

        const ImageRef& shadow_map_image = shadow_map_image_view->GetImage();
        AssertThrow(shadow_map_image.IsValid());

        const ShadowMapAtlasElement& atlas_element = m_shadow_map->GetAtlasElement();

        // Copy combined shadow map to the final shadow map
        frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::COPY_SRC);
        frame->GetCommandList().Add<InsertBarrier>(
            shadow_map_image,
            renderer::ResourceState::COPY_DST,
            renderer::ImageSubResource { .base_array_layer = atlas_element.point_light_index * 6, .num_layers = 6 });

        // copy the image
        for (uint32 i = 0; i < 6; i++)
        {
            frame->GetCommandList().Add<Blit>(
                framebuffer_image,
                shadow_map_image,
                Rect<uint32> { 0, 0, framebuffer_image->GetExtent().x, framebuffer_image->GetExtent().y },
                Rect<uint32> {
                    atlas_element.offset_coords.x,
                    atlas_element.offset_coords.y,
                    atlas_element.offset_coords.x + atlas_element.dimensions.x,
                    atlas_element.offset_coords.y + atlas_element.dimensions.y },
                0,                                      /* src_mip */
                0,                                      /* dst_mip */
                i,                                      /* src_face */
                atlas_element.point_light_index * 6 + i /* dst_face */
            );
        }

        // put the images back into a state for reading
        frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::SHADER_RESOURCE);
        frame->GetCommandList().Add<InsertBarrier>(
            shadow_map_image,
            renderer::ResourceState::SHADER_RESOURCE,
            renderer::ImageSubResource { .base_array_layer = atlas_element.point_light_index * 6, .num_layers = 6 });
    }

    // Temp; refactor
    m_env_probe->SetNeedsRender(false);
}

#pragma endregion RenderEnvProbe

#pragma region EnvProbeRenderer

EnvProbeRenderer::EnvProbeRenderer(EnvProbeType env_probe_type)
    : m_env_probe_type(env_probe_type)
{
    AssertDebug(env_probe_type < EPT_MAX && env_probe_type != EPT_INVALID);
}

EnvProbeRenderer::~EnvProbeRenderer()
{
}

void EnvProbeRenderer::Initialize()
{
}

void EnvProbeRenderer::Shutdown()
{
}

void EnvProbeRenderer::RenderFrame(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.env_probe != nullptr);

    EnvProbe* env_probe = render_setup.env_probe->GetEnvProbe();
    AssertDebug(env_probe != nullptr);

    AssertDebug(env_probe->GetEnvProbeType() == m_env_probe_type);

    RenderSetup rs = render_setup;
    rs.view = env_probe->GetRenderResource().GetViewRenderResourceHandle().Get();

    RenderProbe(frame, rs, env_probe);

    rs.view = nullptr;
}

#pragma endregion EnvProbeRenderer

#pragma region ReflectionProbeRenderer

ReflectionProbeRenderer::ReflectionProbeRenderer()
    : EnvProbeRenderer(EPT_REFLECTION)
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Initialize()
{
    HYP_SCOPE;

    EnvProbeRenderer::Initialize();

    CreateShader();
}

void ReflectionProbeRenderer::Shutdown()
{
    HYP_SCOPE;

    EnvProbeRenderer::Shutdown();

    SafeRelease(std::move(m_shader));
}

void ReflectionProbeRenderer::CreateShader()
{
    HYP_SCOPE;

    AssertDebug(!m_shader.IsValid());

    m_shader = g_shader_manager->GetOrCreate(
        NAME("RenderToCubemap"),
        ShaderProperties(static_mesh_vertex_attributes, { "WRITE_NORMALS", "WRITE_MOMENTS" }));

    AssertThrow(m_shader.IsValid());
}

void ReflectionProbeRenderer::RenderProbe(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = GetConsumerRenderProxyList(view);

    // HYP_LOG(EnvProbe, Debug, "Rendering EnvProbe {} (type: {})",
    //     env_probe->GetID(), env_probe->GetEnvProbeType());

    RenderCollector::ExecuteDrawCalls(frame, render_setup, rpl, ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

    const ViewOutputTarget& output_target = view->GetOutputTarget();
    AssertDebug(output_target.IsValid());

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const ImageRef& framebuffer_image = framebuffer->GetAttachment(0)->GetImage();

    if (env_probe->ShouldComputePrefilteredEnvMap())
    {
        ComputePrefilteredEnvMap(frame, render_setup, env_probe);
    }

    if (env_probe->ShouldComputeSphericalHarmonics())
    {
        ComputeSH(frame, render_setup, env_probe);
    }
}

void ReflectionProbeRenderer::ComputePrefilteredEnvMap(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe)
{
    HYP_SCOPE;

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = GetConsumerRenderProxyList(view);

    struct ConvolveProbeUniforms
    {
        Vec2u out_image_dimensions;
        Vec4f world_position;
        uint32 num_bound_lights;
        alignas(16) uint32 light_indices[16];
    };

    ShaderProperties shader_properties;

    if (!env_probe->IsSkyProbe())
    {
        shader_properties.Set("LIGHTING");
    }

    ShaderRef convolve_probe_shader = g_shader_manager->GetOrCreate(NAME("ConvolveProbe"), shader_properties);

    if (!convolve_probe_shader)
    {
        HYP_FAIL("Failed to create ConvolveProbe shader");
    }

    const Handle<Texture>& prefiltered_env_map = env_probe->GetPrefilteredEnvMap();
    AssertThrow(prefiltered_env_map.IsValid());

    ConvolveProbeUniforms uniforms;
    uniforms.out_image_dimensions = prefiltered_env_map->GetExtent().GetXY();
    uniforms.world_position = env_probe->GetRenderResource().GetBufferData().world_position;

    const uint32 max_bound_lights = ArraySize(uniforms.light_indices);
    uint32 num_bound_lights = 0;

    for (uint32 light_type = 0; light_type < uint32(LT_MAX); light_type++)
    {
        if (num_bound_lights >= max_bound_lights)
        {
            break;
        }

        for (const auto& it : rpl.GetLights(LightType(light_type)))
        {
            HYP_LOG(Rendering, Debug, "Rendering env probe {} : Light bound : {}", env_probe->GetID(), it->GetLight()->GetID());
            if (num_bound_lights >= max_bound_lights)
            {
                break;
            }

            uniforms.light_indices[num_bound_lights++] = it->GetBufferIndex();
        }
    }

    uniforms.num_bound_lights = num_bound_lights;

    GPUBufferRef uniform_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(uniforms));
    HYPERION_ASSERT_RESULT(uniform_buffer->Create());
    uniform_buffer->Copy(sizeof(uniforms), &uniforms);

    const ViewOutputTarget& output_target = view->GetOutputTarget();
    AssertDebug(output_target.IsValid());

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* color_attachment = framebuffer->GetAttachment(0);

    AttachmentBase* normals_attachment = framebuffer->GetAttachment(1);
    AttachmentBase* moments_attachment = framebuffer->GetAttachment(2);

    AssertThrow(color_attachment != nullptr);
    AssertThrow(normals_attachment != nullptr);
    AssertThrow(moments_attachment != nullptr);

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = convolve_probe_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);
    descriptor_table->SetDebugName(NAME_FMT("ConvolveProbeDescriptorTable_{}", env_probe->GetID().Value()));

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("ConvolveProbeDescriptorSet"), frame_index);
        AssertThrow(descriptor_set.IsValid());

        descriptor_set->SetElement(NAME("UniformBuffer"), uniform_buffer);
        descriptor_set->SetElement(NAME("ColorTexture"), color_attachment->GetImageView());
        descriptor_set->SetElement(NAME("NormalsTexture"), normals_attachment ? normals_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("MomentsTexture"), moments_attachment ? moments_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
        descriptor_set->SetElement(NAME("SamplerLinear"), g_render_global_state->PlaceholderData->GetSamplerLinear());
        descriptor_set->SetElement(NAME("SamplerNearest"), g_render_global_state->PlaceholderData->GetSamplerNearest());
        descriptor_set->SetElement(NAME("OutImage"), prefiltered_env_map->GetRenderResource().GetImageView());
    }

    HYPERION_ASSERT_RESULT(descriptor_table->Create());

    ComputePipelineRef convolve_probe_compute_pipeline = g_rendering_api->MakeComputePipeline(convolve_probe_shader, descriptor_table);
    HYPERION_ASSERT_RESULT(convolve_probe_compute_pipeline->Create());

    frame->GetCommandList().Add<InsertBarrier>(prefiltered_env_map->GetRenderResource().GetImage(), renderer::ResourceState::UNORDERED_ACCESS);

    frame->GetCommandList().Add<BindComputePipeline>(convolve_probe_compute_pipeline);

    frame->GetCommandList().Add<BindDescriptorTable>(
        descriptor_table,
        convolve_probe_compute_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"), { { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(render_setup.env_probe, 0) } } } },
        frame->GetFrameIndex());

    frame->GetCommandList().Add<DispatchCompute>(
        convolve_probe_compute_pipeline,
        Vec3u { (prefiltered_env_map->GetExtent().x + 7) / 8, (prefiltered_env_map->GetExtent().y + 7) / 8, 1 });

    if (prefiltered_env_map->GetTextureDesc().HasMipmaps())
    {
        frame->GetCommandList().Add<InsertBarrier>(prefiltered_env_map->GetRenderResource().GetImage(), renderer::ResourceState::COPY_DST);
        frame->GetCommandList().Add<GenerateMipmaps>(prefiltered_env_map->GetRenderResource().GetImage());
    }

    frame->GetCommandList().Add<InsertBarrier>(prefiltered_env_map->GetRenderResource().GetImage(), renderer::ResourceState::SHADER_RESOURCE);

    // for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    // {
    //     g_render_global_state->GlobalDescriptorTable->GetDescriptorSet(NAME("Global"), frame_index)->SetElement(NAME("EnvProbeTextures"), m_texture_slot, prefiltered_env_map->GetRenderResource().GetImageView());
    //     HYP_LOG(EnvProbe, Debug, "Set EnvProbe texture slot {} for envprobe {} in global descriptor table",
    //         m_env_probe->GetTextureSlot(), m_env_probe->GetID());
    // }

    DelegateHandler* delegate_handle = new DelegateHandler();
    *delegate_handle = frame->OnFrameEnd.Bind([delegate_handle, uniform_buffer = std::move(uniform_buffer), convolve_probe_compute_pipeline = std::move(convolve_probe_compute_pipeline), descriptor_table = std::move(descriptor_table)](...)
        {
            HYPERION_ASSERT_RESULT(uniform_buffer->Destroy());
            HYPERION_ASSERT_RESULT(convolve_probe_compute_pipeline->Destroy());
            HYPERION_ASSERT_RESULT(descriptor_table->Destroy());

            delete delegate_handle;
        });
}

void ReflectionProbeRenderer::ComputeSH(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe)
{
    HYP_SCOPE;

    View* view = render_setup.view->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = GetConsumerRenderProxyList(view);

    const ViewOutputTarget& output_target = env_probe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = output_target.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* color_attachment = framebuffer->GetAttachment(0);
    AssertThrow(color_attachment != nullptr);

    AttachmentBase* normals_attachment = framebuffer->GetAttachment(1);
    AttachmentBase* depth_attachment = framebuffer->GetAttachment(2);

    Array<GPUBufferRef> sh_tiles_buffers;
    sh_tiles_buffers.Resize(sh_num_levels);

    Array<DescriptorTableRef> sh_tiles_descriptor_tables;
    sh_tiles_descriptor_tables.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++)
    {
        const SizeType size = sizeof(SHTile) * (sh_num_tiles.x >> i) * (sh_num_tiles.y >> i);

        sh_tiles_buffers[i] = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, size);
        HYPERION_ASSERT_RESULT(sh_tiles_buffers[i]->Create());
    }

    ShaderProperties shader_properties;

    if (!env_probe->IsSkyProbe())
    {
        shader_properties.Set("LIGHTING");
    }

    HashMap<Name, Pair<ShaderRef, ComputePipelineRef>> pipelines = {
        { NAME("Clear"), { g_shader_manager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shader_properties, { { "MODE_CLEAR" } })), ComputePipelineRef() } },
        { NAME("BuildCoeffs"), { g_shader_manager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shader_properties, { { "MODE_BUILD_COEFFICIENTS" } })), ComputePipelineRef() } },
        { NAME("Reduce"), { g_shader_manager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shader_properties, { { "MODE_REDUCE" } })), ComputePipelineRef() } },
        { NAME("Finalize"), { g_shader_manager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shader_properties, { { "MODE_FINALIZE" } })), ComputePipelineRef() } }
    };

    ShaderRef first_shader;

    for (auto& it : pipelines)
    {
        AssertThrow(it.second.first.IsValid());

        if (!first_shader)
        {
            first_shader = it.second.first;
        }
    }

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = first_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    Array<DescriptorTableRef> compute_sh_descriptor_tables;
    compute_sh_descriptor_tables.Resize(sh_num_levels);

    for (uint32 i = 0; i < sh_num_levels; i++)
    {
        compute_sh_descriptor_tables[i] = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& compute_sh_descriptor_set = compute_sh_descriptor_tables[i]->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame_index);
            AssertThrow(compute_sh_descriptor_set != nullptr);

            compute_sh_descriptor_set->SetElement(NAME("InColorCubemap"), color_attachment->GetImageView());
            compute_sh_descriptor_set->SetElement(NAME("InNormalsCubemap"), normals_attachment ? normals_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InDepthCubemap"), depth_attachment ? depth_attachment->GetImageView() : g_render_global_state->PlaceholderData->GetImageViewCube1x1R8());
            compute_sh_descriptor_set->SetElement(NAME("InputSHTilesBuffer"), sh_tiles_buffers[i]);

            if (i != sh_num_levels - 1)
            {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), sh_tiles_buffers[i + 1]);
            }
            else
            {
                compute_sh_descriptor_set->SetElement(NAME("OutputSHTilesBuffer"), sh_tiles_buffers[i]);
            }
        }

        DeferCreate(compute_sh_descriptor_tables[i]);
    }

    for (auto& it : pipelines)
    {
        ComputePipelineRef& pipeline = it.second.second;

        pipeline = g_rendering_api->MakeComputePipeline(
            it.second.first,
            compute_sh_descriptor_tables[0]);

        HYPERION_ASSERT_RESULT(pipeline->Create());
    }

    // Bind a directional light and sky envprobe if available
    RenderEnvProbe* sky_env_probe = nullptr;
    RenderLight* render_light = nullptr;

    if (const Array<RenderLight*>& directional_lights = rpl.GetLights(LT_DIRECTIONAL); directional_lights.Any())
    {
        render_light = directional_lights.Front();
    }

    if (const Array<RenderEnvProbe*>& sky_env_probes = rpl.GetEnvProbes(EPT_SKY); sky_env_probes.Any())
    {
        sky_env_probe = sky_env_probes.Front();
    }

    const Vec2u cubemap_dimensions = color_attachment->GetImage()->GetExtent().GetXY();

    struct
    {
        Vec4u probe_grid_position;
        Vec4u cubemap_dimensions;
        Vec4u level_dimensions;
        Vec4f world_position;
        uint32 env_probe_index;
    } push_constants;

    AssertDebug(env_probe->GetRenderResource().GetBufferIndex() != ~0u);

    push_constants.env_probe_index = env_probe->GetRenderResource().GetBufferIndex();
    push_constants.probe_grid_position = { 0, 0, 0, 0 };
    push_constants.cubemap_dimensions = Vec4u { cubemap_dimensions, 0, 0 };
    push_constants.world_position = env_probe->GetRenderResource().GetBufferData().world_position;

    pipelines[NAME("Clear")].second->SetPushConstants(&push_constants, sizeof(push_constants));
    pipelines[NAME("BuildCoeffs")].second->SetPushConstants(&push_constants, sizeof(push_constants));

    RHICommandList& async_compute_command_list = g_rendering_api->GetAsyncCompute()->GetCommandList();

    async_compute_command_list.Add<InsertBarrier>(sh_tiles_buffers[0], renderer::ResourceState::UNORDERED_ACCESS, renderer::ShaderModuleType::COMPUTE);
    async_compute_command_list.Add<InsertBarrier>(g_render_global_state->EnvProbes->GetBuffer(frame->GetFrameIndex()), renderer::ResourceState::UNORDERED_ACCESS, renderer::ShaderModuleType::COMPUTE);

    async_compute_command_list.Add<BindDescriptorTable>(
        compute_sh_descriptor_tables[0],
        pipelines[NAME("Clear")].second,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(sky_env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(pipelines[NAME("Clear")].second);
    async_compute_command_list.Add<DispatchCompute>(pipelines[NAME("Clear")].second, Vec3u { 1, 1, 1 });

    async_compute_command_list.Add<InsertBarrier>(sh_tiles_buffers[0], renderer::ResourceState::UNORDERED_ACCESS, renderer::ShaderModuleType::COMPUTE);

    async_compute_command_list.Add<BindDescriptorTable>(
        compute_sh_descriptor_tables[0],
        pipelines[NAME("BuildCoeffs")].second,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(sky_env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(pipelines[NAME("BuildCoeffs")].second);
    async_compute_command_list.Add<DispatchCompute>(pipelines[NAME("BuildCoeffs")].second, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (sh_parallel_reduce)
    {
        for (uint32 i = 1; i < sh_num_levels; i++)
        {
            async_compute_command_list.Add<InsertBarrier>(
                sh_tiles_buffers[i - 1],
                renderer::ResourceState::UNORDERED_ACCESS,
                renderer::ShaderModuleType::COMPUTE);

            const Vec2u prev_dimensions {
                MathUtil::Max(1u, sh_num_samples.x >> (i - 1)),
                MathUtil::Max(1u, sh_num_samples.y >> (i - 1))
            };

            const Vec2u next_dimensions {
                MathUtil::Max(1u, sh_num_samples.x >> i),
                MathUtil::Max(1u, sh_num_samples.y >> i)
            };

            AssertThrow(prev_dimensions.x >= 2);
            AssertThrow(prev_dimensions.x > next_dimensions.x);
            AssertThrow(prev_dimensions.y > next_dimensions.y);

            push_constants.level_dimensions = {
                prev_dimensions.x,
                prev_dimensions.y,
                next_dimensions.x,
                next_dimensions.y
            };

            pipelines[NAME("Reduce")].second->SetPushConstants(&push_constants, sizeof(push_constants));

            async_compute_command_list.Add<BindDescriptorTable>(
                compute_sh_descriptor_tables[i - 1],
                pipelines[NAME("Reduce")].second,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("Global"),
                        { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(sky_env_probe, 0) } } } },
                frame->GetFrameIndex());

            async_compute_command_list.Add<BindComputePipeline>(pipelines[NAME("Reduce")].second);
            async_compute_command_list.Add<DispatchCompute>(pipelines[NAME("Reduce")].second, Vec3u { 1, (next_dimensions.x + 3) / 4, (next_dimensions.y + 3) / 4 });
        }
    }

    const uint32 finalize_sh_buffer_index = sh_parallel_reduce ? sh_num_levels - 1 : 0;

    // Finalize - build into final buffer
    async_compute_command_list.Add<InsertBarrier>(sh_tiles_buffers[finalize_sh_buffer_index], renderer::ResourceState::UNORDERED_ACCESS, renderer::ShaderModuleType::COMPUTE);
    async_compute_command_list.Add<InsertBarrier>(g_render_global_state->EnvProbes->GetBuffer(frame->GetFrameIndex()), renderer::ResourceState::UNORDERED_ACCESS, renderer::ShaderModuleType::COMPUTE);

    pipelines[NAME("Finalize")].second->SetPushConstants(&push_constants, sizeof(push_constants));

    async_compute_command_list.Add<BindDescriptorTable>(
        compute_sh_descriptor_tables[finalize_sh_buffer_index],
        pipelines[NAME("Finalize")].second,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(render_light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(sky_env_probe, 0) } } } },
        frame->GetFrameIndex());

    async_compute_command_list.Add<BindComputePipeline>(pipelines[NAME("Finalize")].second);
    async_compute_command_list.Add<DispatchCompute>(pipelines[NAME("Finalize")].second, Vec3u { 1, 1, 1 });

    async_compute_command_list.Add<InsertBarrier>(g_render_global_state->EnvProbes->GetBuffer(frame->GetFrameIndex()), renderer::ResourceState::UNORDERED_ACCESS, renderer::ShaderModuleType::COMPUTE);

    DelegateHandler* delegate_handle = new DelegateHandler();
    *delegate_handle = frame->OnFrameEnd.Bind([render_env_probe = TResourceHandle<RenderEnvProbe>(env_probe->GetRenderResource()), pipelines = std::move(pipelines), descriptor_tables = std::move(compute_sh_descriptor_tables), delegate_handle](FrameBase* frame) mutable
        {
            HYP_NAMED_SCOPE("EnvProbe::ComputeSH - Buffer readback");

            AssertDebug(render_env_probe->GetBufferIndex() != ~0u);

            EnvProbeShaderData readback_buffer;

            g_render_global_state->EnvProbes->ReadbackElement(frame->GetFrameIndex(), render_env_probe->GetBufferIndex(), &readback_buffer);

            Memory::MemCpy(render_env_probe->m_spherical_harmonics.values, readback_buffer.sh.values, sizeof(EnvProbeSphericalHarmonics::values));

            HYP_LOG(EnvProbe, Debug, "EnvProbe {} (type: {}) SH computed", render_env_probe->GetEnvProbe()->GetID(), render_env_probe->GetEnvProbe()->GetEnvProbeType());
            for (uint32 i = 0; i < 9; i++)
            {
                HYP_LOG(EnvProbe, Debug, "SH[{}]: {}", i, render_env_probe->m_spherical_harmonics.values[i]);
            }

            render_env_probe->SetNeedsUpdate();

            for (auto& it : pipelines)
            {
                ShaderRef& shader = it.second.first;
                ComputePipelineRef& pipeline = it.second.second;

                SafeRelease(std::move(shader));
                SafeRelease(std::move(pipeline));
            }

            SafeRelease(std::move(descriptor_tables));

            delete delegate_handle;
        });
}

#pragma endregion ReflectionProbeRenderer

namespace renderer {

HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer, 1, ~0u, false);
HYP_DESCRIPTOR_SSBO(Global, CurrentEnvProbe, 1, sizeof(EnvProbeShaderData), true);

} // namespace renderer

} // namespace hyperion
