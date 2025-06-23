/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/World.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <system/AppContext.hpp>

#include <util/Float16.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::GPUBufferType;

static const InternalFormat env_grid_radiance_format = InternalFormat::RGBA8;
static const InternalFormat env_grid_irradiance_format = InternalFormat::R11G11B10F;
static const Vec2u env_grid_irradiance_extent { 1024, 768 };
static const Vec2u env_grid_radiance_extent { 1024, 768 };

static const Float16 s_ltc_matrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltc_matrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 s_ltc_brdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltc_brdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

void GetDeferredShaderProperties(ShaderProperties& out_shader_properties)
{
    ShaderProperties properties;

    properties.Set("RT_REFLECTIONS_ENABLED", g_rendering_api->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool());
    properties.Set("RT_GI_ENABLED", g_rendering_api->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool());
    properties.Set("ENV_GRID_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.enabled").ToBool());
    properties.Set("HBIL_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool());
    properties.Set("HBAO_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbao.enabled").ToBool());

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.reflections").ToBool())
    {
        properties.Set("DEBUG_REFLECTIONS");
    }
    else if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.irradiance").ToBool())
    {
        properties.Set("DEBUG_IRRADIANCE");
    }

    out_shader_properties = std::move(properties);
}

#pragma region Deferred pass

DeferredPass::DeferredPass(DeferredPassMode mode, GBuffer* gbuffer)
    : FullScreenPass(InternalFormat::RGBA16F, gbuffer),
      m_mode(mode)
{
    SetBlendFunction(BlendFunction::Additive());
}

DeferredPass::~DeferredPass()
{
    SafeRelease(std::move(m_ltc_sampler));
}

void DeferredPass::Create()
{
    CreateShader();

    FullScreenPass::Create();
}

void DeferredPass::CreateShader()
{
    static const FixedArray<ShaderProperties, uint32(LT_MAX)> light_type_properties {
        ShaderProperties { { "LIGHT_TYPE_DIRECTIONAL" } },
        ShaderProperties { { "LIGHT_TYPE_POINT" } },
        ShaderProperties { { "LIGHT_TYPE_SPOT" } },
        ShaderProperties { { "LIGHT_TYPE_AREA_RECT" } }
    };

    switch (m_mode)
    {
    case DeferredPassMode::INDIRECT_LIGHTING:
    {
        ShaderProperties shader_properties;
        GetDeferredShaderProperties(shader_properties);

        m_shader = g_shader_manager->GetOrCreate(NAME("DeferredIndirect"), shader_properties);
        AssertThrow(m_shader.IsValid());

        break;
    }
    case DeferredPassMode::DIRECT_LIGHTING:
        for (uint32 i = 0; i < uint32(LT_MAX); i++)
        {
            ShaderProperties shader_properties;
            GetDeferredShaderProperties(shader_properties);

            shader_properties.Merge(light_type_properties[i]);

            m_direct_light_shaders[i] = g_shader_manager->GetOrCreate(NAME("DeferredDirect"), shader_properties);

            AssertThrow(m_direct_light_shaders[i].IsValid());
        }

        break;
    default:
        HYP_FAIL("Invalid deferred pass mode");
    }
}

void DeferredPass::CreatePipeline(const RenderableAttributeSet& renderable_attributes)
{
    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        FullScreenPass::CreatePipeline(renderable_attributes);
        return;
    }

    { // linear transform cosines texture data
        m_ltc_sampler = g_rendering_api->MakeSampler(
            renderer::FilterMode::TEXTURE_FILTER_NEAREST,
            renderer::FilterMode::TEXTURE_FILTER_LINEAR,
            renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE);

        DeferCreate(m_ltc_sampler);

        ByteBuffer ltc_matrix_data(sizeof(s_ltc_matrix), s_ltc_matrix);

        m_ltc_matrix_texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE },
            std::move(ltc_matrix_data) });

        InitObject(m_ltc_matrix_texture);

        m_ltc_matrix_texture->SetPersistentRenderResourceEnabled(true);

        ByteBuffer ltc_brdf_data(sizeof(s_ltc_brdf), s_ltc_brdf);

        m_ltc_brdf_texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE },
            std::move(ltc_brdf_data) });

        InitObject(m_ltc_brdf_texture);

        m_ltc_brdf_texture->SetPersistentRenderResourceEnabled(true);
    }

    for (uint32 i = 0; i < uint32(LT_MAX); i++)
    {
        ShaderRef& shader = m_direct_light_shaders[i];
        AssertThrow(shader.IsValid());

        const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame_index);
            AssertThrow(descriptor_set.IsValid());

            descriptor_set->SetElement(NAME("MaterialsBuffer"), g_render_global_state->Materials->GetBuffer(frame_index));

            descriptor_set->SetElement(NAME("LTCSampler"), m_ltc_sampler);
            descriptor_set->SetElement(NAME("LTCMatrixTexture"), m_ltc_matrix_texture->GetRenderResource().GetImageView());
            descriptor_set->SetElement(NAME("LTCBRDFTexture"), m_ltc_brdf_texture->GetRenderResource().GetImageView());
        }

        DeferCreate(descriptor_table);

        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            shader,
            renderable_attributes,
            descriptor_table,
            RenderGroupFlags::NONE);

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_direct_light_render_groups[i] = render_group;

        if (i == 0)
        {
            m_render_group = render_group;
        }
    }
}

void DeferredPass::Resize_Internal(Vec2u new_size)
{
    FullScreenPass::Resize_Internal(new_size);
}

void DeferredPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.pass_data != nullptr);

    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        RenderToFramebuffer(frame, rs, nullptr);

        return;
    }

    RenderProxyList& rpl = GetConsumerRenderProxyList(rs.view->GetView());

    // no lights bound, do not render direct shading at all
    if (rpl.NumLights() == 0)
    {
        return;
    }

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    // render with each light
    for (uint32 light_type_index = 0; light_type_index < uint32(LT_MAX); light_type_index++)
    {
        const LightType light_type = LightType(light_type_index);

        const Handle<RenderGroup>& render_group = m_direct_light_render_groups[light_type_index];

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 view_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
        const uint32 material_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
        const uint32 deferred_direct_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DeferredDirectDescriptorSet"));

        render_group->GetPipeline()->SetPushConstants(
            m_push_constant_data.Data(),
            m_push_constant_data.Size());

        frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());

        // Bind textures globally (bindless)
        if (material_descriptor_set_index != ~0u && use_bindless_textures)
        {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {},
                material_descriptor_set_index);
        }

        if (deferred_direct_descriptor_set_index != ~0u)
        {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {},
                deferred_direct_descriptor_set_index);
        }

        const auto& lights = rpl.GetLights(light_type);

        for (RenderLight* render_light : lights)
        {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*rs.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*rs.view->GetCamera()) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(*render_light) } },
                global_descriptor_set_index);

            frame->GetCommandList().Add<BindDescriptorSet>(
                rs.pass_data->descriptor_sets[frame->GetFrameIndex()],
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {},
                view_descriptor_set_index);

            // Bind material descriptor set (for area lights)
            if (material_descriptor_set_index != ~0u && !use_bindless_textures)
            {
                const DescriptorSetRef& material_descriptor_set = render_light->GetMaterial().IsValid()
                    ? render_light->GetMaterial()->GetRenderResource().GetDescriptorSets()[frame->GetFrameIndex()]
                    : g_engine->GetMaterialDescriptorSetManager()->GetInvalidMaterialDescriptorSet(frame->GetFrameIndex());

                AssertThrow(material_descriptor_set != nullptr);

                frame->GetCommandList().Add<BindDescriptorSet>(
                    material_descriptor_set,
                    render_group->GetPipeline(),
                    ArrayMap<Name, uint32> {},
                    material_descriptor_set_index);
            }

            m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());
        }
    }
}

#pragma endregion Deferred pass

#pragma region TonemapPass

TonemapPass::TonemapPass(GBuffer* gbuffer)
    : FullScreenPass(
          InternalFormat::R11G11B10F,
          gbuffer)
{
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::Create()
{
    Threads::AssertOnThread(g_render_thread);

    FullScreenPass::Create();
}

void TonemapPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes },
        MaterialAttributes {
            .fill_mode = FillMode::FILL,
            .blend_function = BlendFunction::None(),
            .flags = MaterialAttributeFlags::NONE });

    m_shader = g_shader_manager->GetOrCreate(NAME("Tonemap"));

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void TonemapPass::Resize_Internal(Vec2u new_size)
{
    FullScreenPass::Resize_Internal(new_size);
}

void TonemapPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
}

#pragma endregion TonemapPass

#pragma region TonemapPass

LightmapPass::LightmapPass(const FramebufferRef& framebuffer, GBuffer* gbuffer)
    : FullScreenPass(
          ShaderRef::Null(),
          DescriptorTableRef::Null(),
          framebuffer,
          InternalFormat::RGBA8,
          framebuffer ? framebuffer->GetExtent() : Vec2u::Zero(),
          gbuffer)
{
}

LightmapPass::~LightmapPass()
{
}

void LightmapPass::Create()
{
    Threads::AssertOnThread(g_render_thread);

    FullScreenPass::Create();
}

void LightmapPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes },
        MaterialAttributes {
            .fill_mode = FillMode::FILL,
            .blend_function = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .flags = MaterialAttributeFlags::NONE });

    m_shader = g_shader_manager->GetOrCreate(NAME("ApplyLightmap"));

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void LightmapPass::Resize_Internal(Vec2u new_size)
{
    FullScreenPass::Resize_Internal(new_size);
}

void LightmapPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    FullScreenPass::Render(frame, rs);
}

#pragma endregion LightmapPass

#pragma region Env grid pass

static ApplyEnvGridMode EnvGridTypeToApplyEnvGridMode(EnvGridType type)
{
    switch (type)
    {
    case EnvGridType::ENV_GRID_TYPE_SH:
        return ApplyEnvGridMode::SH;
    case EnvGridType::ENV_GRID_TYPE_VOXEL:
        return ApplyEnvGridMode::VOXEL;
    case EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD:
        return ApplyEnvGridMode::LIGHT_FIELD;
    default:
        HYP_FAIL("Invalid EnvGridType");
    }
}

EnvGridPass::EnvGridPass(EnvGridPassMode mode, GBuffer* gbuffer)
    : FullScreenPass(
          mode == EnvGridPassMode::RADIANCE
              ? env_grid_radiance_format
              : env_grid_irradiance_format,
          mode == EnvGridPassMode::RADIANCE
              ? env_grid_radiance_extent
              : env_grid_irradiance_extent,
          gbuffer),
      m_mode(mode),
      m_is_first_frame(true)
{
    if (mode == EnvGridPassMode::RADIANCE)
    {
        SetBlendFunction(BlendFunction(
            BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA));
    }
}

EnvGridPass::~EnvGridPass()
{
}

void EnvGridPass::Create()
{
    Threads::AssertOnThread(g_render_thread);

    FullScreenPass::Create();
}

void EnvGridPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes },
        MaterialAttributes {
            .fill_mode = FillMode::FILL,
            .blend_function = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .flags = MaterialAttributeFlags::NONE });

    if (m_mode == EnvGridPassMode::RADIANCE)
    {
        m_shader = g_shader_manager->GetOrCreate(NAME("ApplyEnvGrid"), ShaderProperties { { "MODE_RADIANCE" } });

        FullScreenPass::CreatePipeline(renderable_attributes);

        return;
    }

    static const FixedArray<Pair<ApplyEnvGridMode, ShaderProperties>, uint32(ApplyEnvGridMode::MAX)> apply_env_grid_passes = {
        Pair<ApplyEnvGridMode, ShaderProperties> {
            ApplyEnvGridMode::SH,
            ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_SH" } } },
        Pair<ApplyEnvGridMode, ShaderProperties> {
            ApplyEnvGridMode::VOXEL,
            ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_VOXEL" } } },
        Pair<ApplyEnvGridMode, ShaderProperties> {
            ApplyEnvGridMode::LIGHT_FIELD,
            ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_LIGHT_FIELD" } } }
    };

    for (const auto& it : apply_env_grid_passes)
    {
        ShaderRef shader = g_shader_manager->GetOrCreate(NAME("ApplyEnvGrid"), it.second);
        AssertThrow(shader.IsValid());

        const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);
        DeferCreate(descriptor_table);

        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            shader,
            renderable_attributes,
            descriptor_table,
            RenderGroupFlags::NONE);

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_render_groups[uint32(it.first)] = std::move(render_group);
    }

    m_render_group = m_render_groups[uint32(ApplyEnvGridMode::SH)];
}

void EnvGridPass::Resize_Internal(Vec2u new_size)
{
    FullScreenPass::Resize_Internal(new_size);
}

void EnvGridPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.pass_data != nullptr);

    RenderProxyList& rpl = GetConsumerRenderProxyList(rs.view->GetView());

    // shouldn't be called if no env grids are present
    AssertDebug(rpl.GetEnvGrids().Size() != 0);

    const uint32 frame_index = frame->GetFrameIndex();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

    for (RenderEnvGrid* env_grid : rpl.GetEnvGrids())
    {
        const Handle<RenderGroup>& render_group = m_mode == EnvGridPassMode::RADIANCE
            ? m_render_group
            : m_render_groups[uint32(EnvGridTypeToApplyEnvGridMode(env_grid->GetEnvGrid()->GetEnvGridType()))];

        AssertThrow(render_group.IsValid());

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 view_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (rs.world->GetBufferData().frame_counter & 1);
            const Vec2u viewport_extent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline(), viewport_offset, viewport_extent);
        }
        else
        {
            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());
        }

        frame->GetCommandList().Add<BindDescriptorSet>(
            render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index),
            render_group->GetPipeline(),
            ArrayMap<Name, uint32> {
                { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*rs.world) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*rs.view->GetCamera()) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(*env_grid) } },
            global_descriptor_set_index);

        frame->GetCommandList().Add<BindDescriptorSet>(
            rs.pass_data->descriptor_sets[frame_index],
            render_group->GetPipeline(),
            ArrayMap<Name, uint32> {},
            view_descriptor_set_index);

        m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());
    }

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frame_index);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporal_blending->Render(frame, rs);
    }

    m_is_first_frame = false;
}

#pragma endregion Env grid pass

#pragma region ReflectionsPass

ReflectionsPass::ReflectionsPass(GBuffer* gbuffer, const ImageViewRef& mip_chain_image_view, const ImageViewRef& deferred_result_image_view)
    : FullScreenPass(InternalFormat::R10G10B10A2, gbuffer),
      m_mip_chain_image_view(mip_chain_image_view),
      m_deferred_result_image_view(deferred_result_image_view),
      m_is_first_frame(true)
{
    SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA));
}

ReflectionsPass::~ReflectionsPass()
{
    m_ssr_renderer.Reset();
}

void ReflectionsPass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreateSSRRenderer();
}

void ReflectionsPass::CreatePipeline()
{
    HYP_SCOPE;

    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes },
        MaterialAttributes {
            .fill_mode = FillMode::FILL,
            .blend_function = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .flags = MaterialAttributeFlags::NONE }));
}

void ReflectionsPass::CreatePipeline(const RenderableAttributeSet& renderable_attributes)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // Default pass type (non parallax corrected)

    static const FixedArray<Pair<ApplyReflectionProbeMode, ShaderProperties>, ApplyReflectionProbeMode::MAX> apply_reflection_probe_passes = {
        Pair<ApplyReflectionProbeMode, ShaderProperties> {
            ApplyReflectionProbeMode::DEFAULT,
            ShaderProperties {} },
        Pair<ApplyReflectionProbeMode, ShaderProperties> {
            ApplyReflectionProbeMode::PARALLAX_CORRECTED,
            ShaderProperties { { "ENV_PROBE_PARALLAX_CORRECTED" } } }
    };

    for (const auto& it : apply_reflection_probe_passes)
    {
        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("ApplyReflectionProbe"),
            it.second);

        AssertThrow(shader.IsValid());

        const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);
        DeferCreate(descriptor_table);

        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            shader,
            renderable_attributes,
            descriptor_table,
            RenderGroupFlags::NONE);

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_render_groups[it.first] = std::move(render_group);
    }

    m_render_group = m_render_groups[ApplyReflectionProbeMode::DEFAULT];
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    return g_engine->GetAppContext()->GetConfiguration().Get("rendering.ssr.enabled").ToBool(true)
        && !g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool(false);
}

void ReflectionsPass::CreateSSRRenderer()
{
    m_ssr_renderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig(), m_gbuffer, m_mip_chain_image_view, m_deferred_result_image_view);
    m_ssr_renderer->Create();

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const DescriptorTableDeclaration& descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), m_ssr_renderer->GetFinalResultTexture()->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptor_table);

    m_render_ssr_to_screen_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent,
        m_gbuffer);

    // Use alpha blending to blend SSR into the reflection probes
    m_render_ssr_to_screen_pass->SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA));

    m_render_ssr_to_screen_pass->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(new_size);

    if (ShouldRenderSSR())
    {
        CreateSSRRenderer();
    }
}

void ReflectionsPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(rs.IsValid());
    AssertDebug(rs.HasView());
    AssertDebug(rs.pass_data != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerRenderProxyList(rs.view->GetView());

    if (ShouldRenderSSR())
    {
        m_ssr_renderer->Render(frame, rs);
    }

    // Sky renders first
    static const FixedArray<EnvProbeType, ApplyReflectionProbeMode::MAX> reflection_probe_types {
        EPT_SKY,
        EPT_REFLECTION
    };

    static const FixedArray<ApplyReflectionProbeMode, ApplyReflectionProbeMode::MAX> reflection_probe_modes {
        ApplyReflectionProbeMode::DEFAULT,           // ENV_PROBE_TYPE_SKY
        ApplyReflectionProbeMode::PARALLAX_CORRECTED // ENV_PROBE_TYPE_REFLECTION
    };

    FixedArray<Pair<Handle<RenderGroup>*, Array<RenderEnvProbe*>>, ApplyReflectionProbeMode::MAX> pass_ptrs;

    for (uint32 mode_index = ApplyReflectionProbeMode::DEFAULT; mode_index < ApplyReflectionProbeMode::MAX; mode_index++)
    {
        pass_ptrs[mode_index] = {
            &m_render_groups[mode_index],
            {}
        };

        const EnvProbeType env_probe_type = reflection_probe_types[mode_index];

        for (RenderEnvProbe* render_env_probe : rpl.GetEnvProbes(env_probe_type))
        {
            pass_ptrs[mode_index].second.PushBack(render_env_probe);
        }
    }

    frame->GetCommandList().Add<BeginFramebuffer>(GetFramebuffer(), frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, rs);
    }

    uint32 num_rendered_env_probes = 0;

    for (uint32 reflection_probe_type_index = 0; reflection_probe_type_index < ArraySize(reflection_probe_types); reflection_probe_type_index++)
    {
        const EnvProbeType env_probe_type = reflection_probe_types[reflection_probe_type_index];
        const ApplyReflectionProbeMode mode = reflection_probe_modes[reflection_probe_type_index];

        const Pair<Handle<RenderGroup>*, Array<RenderEnvProbe*>>& it = pass_ptrs[mode];

        if (it.second.Empty())
        {
            continue;
        }

        const Handle<RenderGroup>& render_group = *it.first;
        const Array<RenderEnvProbe*>& env_render_probes = it.second;

        render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());

        if (ShouldRenderHalfRes())
        {
            const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (rs.world->GetBufferData().frame_counter & 1);
            const Vec2u viewport_extent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline(), viewport_offset, viewport_extent);
        }
        else
        {
            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());
        }

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 view_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        for (RenderEnvProbe* env_probe : env_render_probes)
        {
            if (num_rendered_env_probes >= max_bound_reflection_probes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            HYP_LOG(Rendering, Debug, "Applying reflection probe {} (type: {}) with texture slot : {}", env_probe->GetEnvProbe()->GetID(),
                env_probe->GetEnvProbe()->GetEnvProbeType(),
                env_probe->GetTextureSlot());

            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*rs.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*rs.view->GetCamera()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe->GetBufferIndex()) } },
                global_descriptor_set_index);

            frame->GetCommandList().Add<BindDescriptorSet>(
                rs.pass_data->descriptor_sets[frame_index],
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {},
                view_descriptor_set_index);

            m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

            ++num_rendered_env_probes;
        }
    }

    if (ShouldRenderSSR())
    {
        m_render_ssr_to_screen_pass->RenderToFramebuffer(frame, rs, GetFramebuffer());
    }

    frame->GetCommandList().Add<EndFramebuffer>(GetFramebuffer(), frame_index);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporal_blending->Render(frame, rs);
    }

    m_is_first_frame = false;
}

#pragma endregion ReflectionsPass

#pragma region DeferredPassData

DeferredPassData::~DeferredPassData()
{
    SafeRelease(std::move(final_pass_descriptor_set));
    SafeRelease(std::move(descriptor_sets));

    depth_pyramid_renderer.Reset();

    hbao.Reset();

    temporal_aa.Reset();

    // m_dof_blur->Destroy();

    ssgi.Reset();

    post_processing->Destroy();
    post_processing.Reset();

    combine_pass.Reset();

    env_grid_radiance_pass.Reset();
    env_grid_irradiance_pass.Reset();

    reflections_pass.Reset();

    lightmap_pass.Reset();
    tonemap_pass.Reset();
    mip_chain.Reset();
    indirect_pass.Reset();
    direct_pass.Reset();
}

#pragma endregion DeferredPassData

#pragma region DeferredRenderer

constexpr Vec2u mip_chain_extent { 512, 512 };
constexpr InternalFormat mip_chain_format = InternalFormat::R10G10B10A2;

DeferredRenderer::DeferredRenderer()
    : m_renderer_config(RendererConfig::FromConfig())
{
}

DeferredRenderer::~DeferredRenderer()
{
    m_pass_data.Clear();
}

void DeferredRenderer::Initialize()
{
}

void DeferredRenderer::Shutdown()
{
}

void DeferredRenderer::CreateViewPassData(View* view, DeferredPassData& pass_data)
{
    HYP_SCOPE;

    AssertThrow(view != nullptr);
    AssertThrow(view->GetFlags() & ViewFlags::GBUFFER);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    gbuffer->Create();

    HYP_LOG(Rendering, Info, "Creating renderer for view '{}' with GBuffer '{}'", view->GetID(), gbuffer->GetExtent());

    const FramebufferRef& opaque_fbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmap_fbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucent_fbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    pass_data.env_grid_radiance_pass = MakeUnique<EnvGridPass>(EnvGridPassMode::RADIANCE, gbuffer);
    pass_data.env_grid_radiance_pass->Create();

    pass_data.env_grid_irradiance_pass = MakeUnique<EnvGridPass>(EnvGridPassMode::IRRADIANCE, gbuffer);
    pass_data.env_grid_irradiance_pass->Create();

    pass_data.ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig(), gbuffer);
    pass_data.ssgi->Create();

    pass_data.post_processing = MakeUnique<PostProcessing>();
    pass_data.post_processing->Create();

    pass_data.indirect_pass = MakeUnique<DeferredPass>(DeferredPassMode::INDIRECT_LIGHTING, gbuffer);
    pass_data.indirect_pass->Create();

    pass_data.direct_pass = MakeUnique<DeferredPass>(DeferredPassMode::DIRECT_LIGHTING, gbuffer);
    pass_data.direct_pass->Create();

    AttachmentBase* depth_attachment = opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
    AssertThrow(depth_attachment != nullptr);

    pass_data.depth_pyramid_renderer = MakeUnique<DepthPyramidRenderer>(depth_attachment->GetImageView());
    pass_data.depth_pyramid_renderer->Create();

    pass_data.cull_data.depth_pyramid_image_view = pass_data.depth_pyramid_renderer->GetResultImageView();
    pass_data.cull_data.depth_pyramid_dimensions = pass_data.depth_pyramid_renderer->GetExtent();

    pass_data.mip_chain = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        mip_chain_format,
        Vec3u { mip_chain_extent.x, mip_chain_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE });

    InitObject(pass_data.mip_chain);

    pass_data.mip_chain->SetPersistentRenderResourceEnabled(true);

    pass_data.hbao = MakeUnique<HBAO>(HBAOConfig::FromConfig(), gbuffer);
    pass_data.hbao->Create();

    // m_dof_blur = MakeUnique<DOFBlur>(gbuffer->GetResolution(), gbuffer);
    // m_dof_blur->Create();

    CreateViewCombinePass(view, pass_data);

    pass_data.reflections_pass = MakeUnique<ReflectionsPass>(gbuffer, pass_data.mip_chain->GetRenderResource().GetImageView(), pass_data.combine_pass->GetFinalImageView());
    pass_data.reflections_pass->Create();

    pass_data.tonemap_pass = MakeUnique<TonemapPass>(gbuffer);
    pass_data.tonemap_pass->Create();

    // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
    pass_data.lightmap_pass = MakeUnique<LightmapPass>(translucent_fbo, gbuffer);
    pass_data.lightmap_pass->Create();

    pass_data.temporal_aa = MakeUnique<TemporalAA>(gbuffer->GetExtent(), pass_data.tonemap_pass->GetFinalImageView(), gbuffer);
    pass_data.temporal_aa->Create();

    CreateViewDescriptorSets(view, pass_data);
    CreateViewFinalPassDescriptorSet(view, pass_data);
}

void DeferredRenderer::CreateViewFinalPassDescriptorSet(View* view, DeferredPassData& pass_data)
{
    HYP_SCOPE;

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const ImageViewRef& input_image_view = m_renderer_config.taa_enabled
        ? pass_data.temporal_aa->GetResultTexture()->GetRenderResource().GetImageView()
        : pass_data.combine_pass->GetFinalImageView();

    AssertThrow(input_image_view.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    renderer::DescriptorSetDeclaration* decl = descriptor_table_decl.FindDescriptorSetDeclaration(NAME("RenderTextureToScreenDescriptorSet"));
    AssertThrow(decl != nullptr);

    const renderer::DescriptorSetLayout layout { decl };

    DescriptorSetRef descriptor_set = g_rendering_api->MakeDescriptorSet(layout);
    descriptor_set->SetDebugName(NAME("FinalPassDescriptorSet"));
    descriptor_set->SetElement(NAME("InTexture"), input_image_view);

    DeferCreate(descriptor_set);

    pass_data.final_pass_descriptor_set = std::move(descriptor_set);
}

void DeferredRenderer::CreateViewDescriptorSets(View* view, DeferredPassData& pass_data)
{
    HYP_SCOPE;

    const renderer::DescriptorSetDeclaration* decl = g_render_global_state->GlobalDescriptorTable->GetDeclaration()->FindDescriptorSetDeclaration(NAME("View"));
    AssertThrow(decl != nullptr);

    const renderer::DescriptorSetLayout layout { decl };

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    static const FixedArray<Name, GBUFFER_RESOURCE_MAX> gbuffer_texture_names {
        NAME("GBufferAlbedoTexture"),
        NAME("GBufferNormalsTexture"),
        NAME("GBufferMaterialTexture"),
        NAME("GBufferLightmapTexture"),
        NAME("GBufferVelocityTexture"),
        NAME("GBufferMaskTexture"),
        NAME("GBufferWSNormalsTexture"),
        NAME("GBufferTranslucentTexture")
    };

    const FramebufferRef& opaque_fbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmap_fbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucent_fbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    // depth attachment goes into separate slot
    AttachmentBase* depth_attachment = opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
    AssertThrow(depth_attachment != nullptr);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        DescriptorSetRef descriptor_set = g_rendering_api->MakeDescriptorSet(layout);
        descriptor_set->SetDebugName(NAME_FMT("SceneViewDescriptorSet_{}", frame_index));

        if (g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported())
        {
            uint32 gbuffer_element_index = 0;

            // not including depth texture here (hence the - 1)
            for (uint32 attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++)
            {
                descriptor_set->SetElement(NAME("GBufferTextures"), gbuffer_element_index++, opaque_fbo->GetAttachment(attachment_index)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptor_set->SetElement(NAME("GBufferTextures"), gbuffer_element_index++, translucent_fbo->GetAttachment(0)->GetImageView());
        }
        else
        {
            for (uint32 attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++)
            {
                descriptor_set->SetElement(gbuffer_texture_names[attachment_index], opaque_fbo->GetAttachment(attachment_index)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptor_set->SetElement(NAME("GBufferTranslucentTexture"), translucent_fbo->GetAttachment(0)->GetImageView());
        }

        descriptor_set->SetElement(NAME("GBufferDepthTexture"), depth_attachment->GetImageView());

        descriptor_set->SetElement(NAME("GBufferMipChain"), pass_data.mip_chain->GetRenderResource().GetImageView());

        descriptor_set->SetElement(NAME("PostProcessingUniforms"), pass_data.post_processing->GetUniformBuffer());

        descriptor_set->SetElement(NAME("DepthPyramidResult"), pass_data.depth_pyramid_renderer->GetResultImageView());

        descriptor_set->SetElement(NAME("TAAResultTexture"), pass_data.temporal_aa->GetResultTexture()->GetRenderResource().GetImageView());

        if (pass_data.reflections_pass->ShouldRenderSSR())
        {
            descriptor_set->SetElement(NAME("SSRResultTexture"), pass_data.reflections_pass->GetSSRRenderer()->GetFinalResultTexture()->GetRenderResource().GetImageView());
        }
        else
        {
            descriptor_set->SetElement(NAME("SSRResultTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        if (pass_data.ssgi)
        {
            descriptor_set->SetElement(NAME("SSGIResultTexture"), pass_data.ssgi->GetFinalResultTexture()->GetRenderResource().GetImageView());
        }
        else
        {
            descriptor_set->SetElement(NAME("SSGIResultTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        if (pass_data.hbao)
        {
            descriptor_set->SetElement(NAME("SSAOResultTexture"), pass_data.hbao->GetFinalImageView());
        }
        else
        {
            descriptor_set->SetElement(NAME("SSAOResultTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }

        descriptor_set->SetElement(NAME("DeferredResult"), pass_data.combine_pass->GetFinalImageView());

        descriptor_set->SetElement(NAME("DeferredIndirectResultTexture"), pass_data.indirect_pass->GetFinalImageView());

        descriptor_set->SetElement(NAME("ReflectionProbeResultTexture"), pass_data.reflections_pass->GetFinalImageView());

        descriptor_set->SetElement(NAME("EnvGridRadianceResultTexture"), pass_data.env_grid_radiance_pass->GetFinalImageView());
        descriptor_set->SetElement(NAME("EnvGridIrradianceResultTexture"), pass_data.env_grid_irradiance_pass->GetFinalImageView());

        HYPERION_ASSERT_RESULT(descriptor_set->Create());

        descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    pass_data.descriptor_sets = std::move(descriptor_sets);
}

void DeferredRenderer::CreateViewCombinePass(View* view, DeferredPassData& pass_data)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // The combine pass will render into the translucent bucket's framebuffer with the shaded result.
    const FramebufferRef& translucent_fbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    AssertThrow(translucent_fbo != nullptr);

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), pass_data.indirect_pass->GetFinalImageView());
    }

    DeferCreate(descriptor_table);

    pass_data.combine_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        translucent_fbo,
        translucent_fbo->GetAttachment(0)->GetFormat(),
        translucent_fbo->GetExtent(),
        nullptr);

    pass_data.combine_pass->Create();
}

void DeferredRenderer::ResizeView(Viewport viewport, View* view, DeferredPassData& pass_data)
{
    AssertThrow(viewport.extent.Volume() > 0);

    const Vec2u new_size = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    gbuffer->Resize(new_size);

    const FramebufferRef& opaque_fbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmap_fbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucent_fbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    pass_data.hbao->Resize(new_size);

    pass_data.direct_pass->Resize(new_size);
    pass_data.indirect_pass->Resize(new_size);

    pass_data.combine_pass.Reset();
    CreateViewCombinePass(view, pass_data);

    pass_data.env_grid_radiance_pass->Resize(new_size);
    pass_data.env_grid_irradiance_pass->Resize(new_size);

    pass_data.reflections_pass->Resize(new_size);

    pass_data.tonemap_pass->Resize(new_size);

    pass_data.lightmap_pass = MakeUnique<LightmapPass>(translucent_fbo, gbuffer);
    pass_data.lightmap_pass->Create();

    pass_data.temporal_aa = MakeUnique<TemporalAA>(new_size, pass_data.tonemap_pass->GetFinalImageView(), gbuffer);
    pass_data.temporal_aa->Create();

    AttachmentBase* depth_attachment = opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
    AssertThrow(depth_attachment != nullptr);

    pass_data.depth_pyramid_renderer = MakeUnique<DepthPyramidRenderer>(depth_attachment->GetImageView());
    pass_data.depth_pyramid_renderer->Create();

    SafeRelease(std::move(pass_data.descriptor_sets));
    CreateViewDescriptorSets(view, pass_data);

    SafeRelease(std::move(pass_data.final_pass_descriptor_set));
    CreateViewFinalPassDescriptorSet(view, pass_data);

    pass_data.viewport = viewport;
}

void DeferredRenderer::RenderFrame(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertThrow(rs.IsValid());

    EngineRenderStatsCounts counts {};

    Array<RenderProxyList*> render_proxy_lists;

    // Collect view-independent renderable types from all views
    FixedArray<Array<RenderLight*>, uint32(LT_MAX)> lights;
    FixedArray<Array<RenderEnvProbe*>, uint32(EPT_REFLECTION) + 1> env_probes;
    Array<RenderEnvGrid*> env_grids;

    for (const TResourceHandle<RenderView>& render_view : rs.world->GetViews())
    {
        AssertThrow(render_view);

        View* view = render_view->GetView();

        if (!view)
        {
            continue;
        }

        RenderProxyList& rpl = GetConsumerRenderProxyList(view);
        render_proxy_lists.PushBack(&rpl);

        DeferredPassData*& pd = m_view_pass_data[view];

        if (!pd)
        {
            pd = &m_pass_data.EmplaceBack();

            CreateViewPassData(view, *pd);

            pd->viewport = view->GetRenderResource().GetViewport(); // rpl.viewport;
        }
        else if (pd->viewport != view->GetRenderResource().GetViewport())
        {
            ResizeView(view->GetRenderResource().GetViewport(), view, *pd);
        }

        pd->priority = view->GetRenderResource().GetPriority();

        for (uint32 light_type = 0; light_type < uint32(LT_MAX); light_type++)
        {
            for (RenderLight* light : rpl.GetLights(LightType(light_type)))
            {
                if (lights[light_type].Contains(light))
                {
                    continue;
                }

                lights[light_type].PushBack(light);
            }
        }

        for (RenderEnvGrid* env_grid : rpl.GetEnvGrids())
        {
            if (env_grids.Contains(env_grid))
            {
                continue;
            }

            env_grids.PushBack(env_grid);
        }

        for (uint32 env_probe_type = 0; env_probe_type <= uint32(EPT_REFLECTION); env_probe_type++)
        {
            for (RenderEnvProbe* env_probe : rpl.GetEnvProbes(EnvProbeType(env_probe_type)))
            {
                if (env_probes[env_probe_type].Contains(env_probe))
                {
                    continue;
                }

                env_probes[env_probe_type].PushBack(env_probe);
            }
        }
    }

    // Render global environment probes and grids and set fallbacks
    RenderSetup new_rs = rs;

    {
        // Set global directional light as fallback
        if (lights[uint32(LT_DIRECTIONAL)].Any())
        {
            new_rs.light = lights[uint32(LT_DIRECTIONAL)][0];
        }
        else
        {
            HYP_LOG(Rendering, Warning, "No directional light found in the world! EnvGrid and EnvProbe will have no fallback light set.");
            new_rs.light = nullptr;
        }

        // Set sky as fallback probe
        if (env_probes[uint32(EPT_SKY)].Any())
        {
            new_rs.env_probe = env_probes[uint32(EPT_SKY)][0];
        }

        if (env_probes.Any())
        {
            for (uint32 env_probe_type = 0; env_probe_type <= EPT_REFLECTION; env_probe_type++)
            {
                for (RenderEnvProbe* env_probe : env_probes[env_probe_type])
                {
                    HYP_LOG(Rendering, Debug, "Binding EnvProbe {} (type: {})", env_probe->GetEnvProbe()->GetID(),
                        env_probe->GetEnvProbe()->GetEnvProbeType());
                    // Acquire binding slot for the env probe
                    if (!g_render_global_state->EnvProbeBinder.Bind(env_probe->GetEnvProbe()))
                    {
                        HYP_LOG(Rendering, Warning, "Failed to bind EnvProbe {}! Skipping rendering of env probes of this type.", env_probe->GetEnvProbe()->GetID());
                        continue;
                    }

                    // if (env_probe->GetEnvProbe()->NeedsRender())
                    // {
                    EnvProbeRenderer* env_probe_renderer = g_render_global_state->EnvProbeRenderers[env_probe_type];
                    if (env_probe_renderer)
                    {
                        new_rs.env_probe = env_probe;
                        env_probe_renderer->RenderFrame(frame, new_rs);
                        new_rs.env_probe = nullptr; // reset for next probe

                        counts[ERS_ENV_PROBES]++;
                    }
                    else
                    {

                        HYP_LOG(Rendering, Warning, "No EnvProbeRenderer found for EnvProbeType {}! Skipping rendering of env probes of this type.", EPT_REFLECTION);
                    }
                    // }
                }
            }
        }

        if (env_grids.Any())
        {
            for (RenderEnvGrid* env_grid : env_grids)
            {
                env_grid->Render(frame, new_rs);

                counts[ERS_ENV_GRIDS]++;
            }
        }
    }

    for (const TResourceHandle<RenderView>& render_view : rs.world->GetViews())
    {
        AssertThrow(render_view);

        View* view = render_view->GetView();

        if (!view)
        {
            continue;
        }

        DeferredPassData* pd = m_view_pass_data[view];
        AssertDebug(pd != nullptr);

        new_rs.view = render_view.Get();
        new_rs.pass_data = pd;

        RenderFrameForView(frame, new_rs);

        new_rs.view = nullptr;
        new_rs.pass_data = nullptr;

#ifdef HYP_ENABLE_RENDER_STATS
        RenderProxyList& rpl = GetConsumerRenderProxyList(view);

        counts[ERS_VIEWS]++;
        counts[ERS_LIGHTMAP_VOLUMES] += rpl.lightmap_volumes.Size();
        counts[ERS_LIGHTS] += rpl.NumLights();
        counts[ERS_ENV_GRIDS] += rpl.env_grids.Size();
        counts[ERS_ENV_PROBES] += rpl.NumEnvProbes();
#endif
    }

    g_engine->GetRenderStatsCalculator().AddCounts(counts);
}

void DeferredRenderer::RenderFrameForView(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertThrow(rs.IsValid());
    AssertThrow(rs.HasView());

    uint32 global_frame_index = GetRenderThreadFrameIndex();
    if (m_last_frame_data.frame_id != global_frame_index)
    {
        m_last_frame_data.frame_id = global_frame_index;
        m_last_frame_data.pass_data.Clear();
    }

    View* view = rs.view->GetView();

    if (!(view->GetFlags() & ViewFlags::GBUFFER))
    {
        return;
    }

    RenderProxyList& rpl = GetConsumerRenderProxyList(view);

    DeferredPassData* pd = static_cast<DeferredPassData*>(rs.pass_data);
    AssertDebug(pd != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    // @TODO: Refactor to put this in the RenderWorld
    RenderEnvironment* environment = rs.world->GetEnvironment();

    const FramebufferRef& opaque_fbo = view->GetOutputTarget().GetFramebuffer(RB_OPAQUE);
    const FramebufferRef& lightmap_fbo = view->GetOutputTarget().GetFramebuffer(RB_LIGHTMAP);
    const FramebufferRef& translucent_fbo = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);

    const bool do_particles = true;
    const bool do_gaussian_splatting = false; // environment && environment->IsReady();

    const bool use_rt_radiance = m_renderer_config.path_tracer_enabled || m_renderer_config.rt_reflections_enabled;
    const bool use_ddgi = m_renderer_config.rt_gi_enabled;
    const bool use_hbao = m_renderer_config.hbao_enabled;
    const bool use_hbil = m_renderer_config.hbil_enabled;
    const bool use_ssgi = m_renderer_config.ssgi_enabled;

    const bool use_env_grid_irradiance = rpl.env_grids.Any() && m_renderer_config.env_grid_gi_enabled;
    const bool use_env_grid_radiance = rpl.env_grids.Any() && m_renderer_config.env_grid_radiance_enabled;

    const bool use_temporal_aa = pd->temporal_aa != nullptr && m_renderer_config.taa_enabled;

    const bool use_reflection_probes = rpl.env_probes[uint32(EPT_SKY)].Any() || rpl.env_probes[uint32(EPT_REFLECTION)].Any();

    if (use_temporal_aa)
    {
        view->GetRenderResource().m_render_camera->ApplyJitter(rs);
    }

    struct
    {
        uint32 flags;
        uint32 screen_width;
        uint32 screen_height;
    } deferred_data;

    Memory::MemSet(&deferred_data, 0, sizeof(deferred_data));

    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferred_data.flags |= use_ddgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;

    deferred_data.screen_width = view->GetRenderResource().GetViewport().extent.x;  // rpl.viewport.extent.x;
    deferred_data.screen_height = view->GetRenderResource().GetViewport().extent.y; // rpl.viewport.extent.y;

    PerformOcclusionCulling(frame, rs);

    if (do_particles)
    {
        environment->GetParticleSystem()->UpdateParticles(frame, rs);
    }

    if (do_gaussian_splatting)
    {
        environment->GetGaussianSplatting()->UpdateSplats(frame, rs);
    }

    pd->indirect_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
    pd->direct_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));

    { // render opaque objects into separate framebuffer
        frame->GetCommandList().Add<BeginFramebuffer>(opaque_fbo, frame_index);

        ExecuteDrawCalls(frame, rs, (1u << RB_OPAQUE));

        frame->GetCommandList().Add<EndFramebuffer>(opaque_fbo, frame_index);
    }

    if (use_env_grid_irradiance || use_env_grid_radiance)
    {
        if (use_env_grid_irradiance)
        {
            pd->env_grid_irradiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
            pd->env_grid_irradiance_pass->Render(frame, rs);
        }

        if (use_env_grid_radiance)
        {
            pd->env_grid_radiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
            pd->env_grid_radiance_pass->Render(frame, rs);
        }
    }

    if (use_reflection_probes)
    {
        pd->reflections_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        pd->reflections_pass->Render(frame, rs);
    }

    // @FIXME: RT Radiance should be moved away from the RenderEnvironment and be part of the RenderView,
    // otherwise the same pass will be executed for each view (shared).
    // DDGI Should be a RenderSubsystem instead of existing on the RenderEnvironment directly,
    // so it is rendered with the RenderWorld rather than the RenderView.
    if (use_rt_radiance)
    {
        environment->RenderRTRadiance(frame, rs);
    }

    if (use_ddgi)
    {
        environment->RenderDDGIProbes(frame, rs);
    }

    if (use_hbao || use_hbil)
    {
        pd->hbao->Render(frame, rs);
    }

    if (use_ssgi)
    {
        RenderSetup new_render_setup = rs;

        if (const Array<RenderEnvProbe*>& sky_env_probes = rpl.env_probes[uint32(EPT_SKY)]; sky_env_probes.Any())
        {
            new_render_setup.env_probe = sky_env_probes.Front();
        }

        pd->ssgi->Render(frame, rs);
    }

    pd->post_processing->RenderPre(frame, rs);

    // render indirect and direct lighting into the same framebuffer
    const FramebufferRef& deferred_pass_framebuffer = pd->indirect_pass->GetFramebuffer();

    { // deferred lighting on opaque objects
        frame->GetCommandList().Add<BeginFramebuffer>(deferred_pass_framebuffer, frame_index);

        pd->indirect_pass->Render(frame, rs);
        pd->direct_pass->Render(frame, rs);

        frame->GetCommandList().Add<EndFramebuffer>(deferred_pass_framebuffer, frame_index);
    }

    if (rpl.lightmap_volumes.Any())
    { // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        frame->GetCommandList().Add<BeginFramebuffer>(lightmap_fbo, frame_index);

        ExecuteDrawCalls(frame, rs, (1u << RB_LIGHTMAP));

        frame->GetCommandList().Add<EndFramebuffer>(lightmap_fbo, frame_index);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const ImageRef& src_image = deferred_pass_framebuffer->GetAttachment(0)->GetImage();
        GenerateMipChain(frame, rs, src_image);
    }

    { // translucent objects
        frame->GetCommandList().Add<BeginFramebuffer>(translucent_fbo, frame_index);

        { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.
            frame->GetCommandList().Add<BindGraphicsPipeline>(pd->combine_pass->GetRenderGroup()->GetPipeline());

            frame->GetCommandList().Add<BindDescriptorTable>(
                pd->combine_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
                pd->combine_pass->GetRenderGroup()->GetPipeline(),
                ArrayMap<Name, ArrayMap<Name, uint32>> {},
                frame_index);

            pd->combine_pass->GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
        }

        // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
        // Apply lightmaps over the now shaded opaque objects.
        pd->lightmap_pass->RenderToFramebuffer(frame, rs, translucent_fbo);

        // begin translucent with forward rendering
        ExecuteDrawCalls(frame, rs, (1u << RB_TRANSLUCENT));
        ExecuteDrawCalls(frame, rs, (1u << RB_DEBUG));

        // if (do_particles)
        // {
        //     environment->GetParticleSystem()->Render(frame, rs);
        // }

        // if (do_gaussian_splatting)
        // {
        //     environment->GetGaussianSplatting()->Render(frame, rs);
        // }

        ExecuteDrawCalls(frame, rs, (1u << RB_SKYBOX));

        // // render debug draw
        // g_engine->GetDebugDrawer()->Render(frame, rs);

        frame->GetCommandList().Add<EndFramebuffer>(translucent_fbo, frame_index);
    }

    { // render depth pyramid
        pd->depth_pyramid_renderer->Render(frame);
        // update culling info now that depth pyramid has been rendered
        pd->cull_data.depth_pyramid_image_view = pd->depth_pyramid_renderer->GetResultImageView();
        pd->cull_data.depth_pyramid_dimensions = pd->depth_pyramid_renderer->GetExtent();
    }

    pd->post_processing->RenderPost(frame, rs);

    pd->tonemap_pass->Render(frame, rs);

    if (use_temporal_aa)
    {
        pd->temporal_aa->Render(frame, rs);
    }

    // depth of field
    // m_dof_blur->Render(frame);

    // Ordered by View priority
    auto last_frame_data_it = std::lower_bound(
        m_last_frame_data.pass_data.Begin(),
        m_last_frame_data.pass_data.End(),
        Pair<View*, DeferredPassData*> { view, pd },
        [view](const Pair<View*, DeferredPassData*>& a, const Pair<View*, DeferredPassData*>& b)
        {
            return a.second->priority < b.second->priority;
        });

    m_last_frame_data.pass_data.Insert(last_frame_data_it, Pair<View*, DeferredPassData*> { view, pd });
}

void DeferredRenderer::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    constexpr uint32 bucket_mask = (1 << RB_OPAQUE)
        | (1 << RB_LIGHTMAP)
        | (1 << RB_SKYBOX)
        | (1 << RB_TRANSLUCENT)
        | (1 << RB_DEBUG);

    RenderCollector::PerformOcclusionCulling(frame, rs, GetConsumerRenderProxyList(rs.view->GetView()), bucket_mask);
}

void DeferredRenderer::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& rs, uint32 bucket_mask)
{
    HYP_SCOPE;

    RenderCollector::ExecuteDrawCalls(frame, rs, GetConsumerRenderProxyList(rs.view->GetView()), bucket_mask);
}

void DeferredRenderer::GenerateMipChain(FrameBase* frame, const RenderSetup& rs, const ImageRef& src_image)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    DeferredPassData* pd = static_cast<DeferredPassData*>(rs.pass_data);

    const ImageRef& mipmapped_result = pd->mip_chain->GetRenderResource().GetImage();
    AssertThrow(mipmapped_result.IsValid());

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(mipmapped_result, renderer::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    frame->GetCommandList().Add<BlitRect>(
        src_image,
        mipmapped_result,
        Rect<uint32> { 0, 0, src_image->GetExtent().x, src_image->GetExtent().y },
        Rect<uint32> { 0, 0, mipmapped_result->GetExtent().x, mipmapped_result->GetExtent().y });

    frame->GetCommandList().Add<GenerateMipmaps>(mipmapped_result);

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::SHADER_RESOURCE);
}

#pragma endregion DeferredRenderer

} // namespace hyperion
