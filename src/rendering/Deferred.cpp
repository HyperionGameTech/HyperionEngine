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
#include <rendering/ShaderGlobals.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderState.hpp>
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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <system/AppContext.hpp>

#include <util/Float16.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

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

    properties.Set(
        "RT_REFLECTIONS_ENABLED",
        g_rendering_api->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool());

    properties.Set(
        "RT_GI_ENABLED",
        g_rendering_api->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool());

    properties.Set(
        "ENV_GRID_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.enabled").ToBool());

    properties.Set(
        "HBIL_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool());

    properties.Set(
        "HBAO_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbao.enabled").ToBool());

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
    static const FixedArray<ShaderProperties, uint32(LightType::MAX)> light_type_properties {
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
        for (uint32 i = 0; i < uint32(LightType::MAX); i++)
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

    for (uint32 i = 0; i < uint32(LightType::MAX); i++)
    {
        ShaderRef& shader = m_direct_light_shaders[i];
        AssertThrow(shader.IsValid());

        const renderer::DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame_index);
            AssertThrow(descriptor_set.IsValid());

            descriptor_set->SetElement(NAME("MaterialsBuffer"), g_engine->GetRenderData()->materials->GetBuffer(frame_index));

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

void DeferredPass::Render(FrameBase* frame, RenderView* view)
{
    HYP_SCOPE;

    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING)
    {
        RenderToFramebuffer(frame, view, nullptr);

        return;
    }

    // no lights bound, do not render direct shading at all
    if (view->NumLights() == 0)
    {
        return;
    }

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    const TResourceHandle<RenderEnvProbe>& env_render_probe = g_engine->GetRenderState()->GetActiveEnvProbe();
    const TResourceHandle<RenderEnvGrid>& env_render_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    // render with each light
    for (uint32 light_type_index = 0; light_type_index < uint32(LightType::MAX); light_type_index++)
    {
        const LightType light_type = LightType(light_type_index);

        const Handle<RenderGroup>& render_group = m_direct_light_render_groups[light_type_index];

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
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

        const auto& lights = view->GetLights(light_type);

        for (RenderLight* render_light : lights)
        {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(*view->GetScene()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_render_grid.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_render_probe.Get(), 0) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(*render_light) } },
                global_descriptor_set_index);

            frame->GetCommandList().Add<BindDescriptorSet>(
                view->GetDescriptorSets()[frame->GetFrameIndex()],
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {},
                scene_descriptor_set_index);

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

void TonemapPass::Render(FrameBase* frame, RenderView* view)
{
    FullScreenPass::Render(frame, view);
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

void LightmapPass::Render(FrameBase* frame, RenderView* view)
{
    FullScreenPass::Render(frame, view);
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

void EnvGridPass::Render(FrameBase* frame, RenderView* view)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const TResourceHandle<RenderEnvGrid>& env_render_grid = g_engine->GetRenderState()->GetActiveEnvGrid();
    AssertThrow(env_render_grid);

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, view);
    }

    const Handle<RenderGroup>& render_group = m_mode == EnvGridPassMode::RADIANCE
        ? m_render_group
        : m_render_groups[uint32(EnvGridTypeToApplyEnvGridMode(env_render_grid->GetEnvGrid()->GetEnvGridType()))];

    AssertThrow(render_group.IsValid());

    const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const uint32 scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

    render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

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
            { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(*view->GetScene()) },
            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*view->GetCamera()) },
            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(*env_render_grid) } },
        global_descriptor_set_index);

    frame->GetCommandList().Add<BindDescriptorSet>(
        view->GetDescriptorSets()[frame_index],
        render_group->GetPipeline(),
        ArrayMap<Name, uint32> {},
        scene_descriptor_set_index);

    m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frame_index);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, view);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, view);
        }

        m_temporal_blending->Render(frame, view);
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

void ReflectionsPass::Render(FrameBase* frame, RenderView* view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    if (ShouldRenderSSR())
    {
        m_ssr_renderer->Render(frame, view);
    }

    // Sky renders first
    static const FixedArray<EnvProbeType, ApplyReflectionProbeMode::MAX> reflection_probe_types {
        ENV_PROBE_TYPE_SKY,
        ENV_PROBE_TYPE_REFLECTION
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

        for (const TResourceHandle<RenderEnvProbe>& resource_handle : g_engine->GetRenderState()->bound_env_probes[env_probe_type])
        {
            pass_ptrs[mode_index].second.PushBack(resource_handle.Get());
        }
    }

    frame->GetCommandList().Add<BeginFramebuffer>(GetFramebuffer(), frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, view);
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
            const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
            const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline(), viewport_offset, viewport_extent);
        }
        else
        {
            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());
        }

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

        for (RenderEnvProbe* env_render_probe : env_render_probes)
        {
            if (num_rendered_env_probes >= max_bound_reflection_probes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(*view->GetScene()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*view->GetCamera()) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_render_probe->GetBufferIndex()) } },
                global_descriptor_set_index);

            frame->GetCommandList().Add<BindDescriptorSet>(
                view->GetDescriptorSets()[frame_index],
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {},
                scene_descriptor_set_index);

            m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

            ++num_rendered_env_probes;
        }
    }

    if (ShouldRenderSSR())
    {
        m_render_ssr_to_screen_pass->RenderToFramebuffer(frame, view, GetFramebuffer());
    }

    frame->GetCommandList().Add<EndFramebuffer>(GetFramebuffer(), frame_index);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, view);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, view);
        }

        m_temporal_blending->Render(frame, view);
    }

    m_is_first_frame = false;
}

#pragma endregion ReflectionsPass

} // namespace hyperion
