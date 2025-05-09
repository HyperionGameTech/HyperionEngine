/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/EnvGrid.hpp>
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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <system/AppContext.hpp>

#include <util/BlueNoise.hpp>

#include <util/Float16.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::GPUBufferType;

static const Vec2u mip_chain_extent { 512, 512 };
static const InternalFormat mip_chain_format = InternalFormat::R10G10B10A2;

static const InternalFormat env_grid_radiance_format = InternalFormat::RGBA8;
static const InternalFormat env_grid_irradiance_format = InternalFormat::R11G11B10F;
static const Vec2u env_grid_irradiance_extent { 1024, 768 };
static const Vec2u env_grid_radiance_extent { 1024, 768 };

static const float16 s_ltc_matrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltc_matrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const float16 s_ltc_brdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltc_brdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

#pragma region Render commands

struct RENDER_COMMAND(SetDeferredResultInGlobalDescriptorSet) : renderer::RenderCommand
{
    ImageViewRef    result_image_view;

    RENDER_COMMAND(SetDeferredResultInGlobalDescriptorSet)(
        ImageViewRef result_image_view
    ) : result_image_view(std::move(result_image_view))
    {
    }

    virtual ~RENDER_COMMAND(SetDeferredResultInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DeferredResult"), result_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

static ShaderProperties GetDeferredShaderProperties()
{
    ShaderProperties properties;

    properties.Set(
        "RT_REFLECTIONS_ENABLED",
        g_rendering_api->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool()
    );

    properties.Set(
        "RT_GI_ENABLED",
        g_rendering_api->GetRenderConfig().IsRaytracingSupported() && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool()
    );
    
    properties.Set(
        "ENV_GRID_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.enabled").ToBool()
    );

    properties.Set(
        "HBIL_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool()
    );

    properties.Set(
        "HBAO_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbao.enabled").ToBool()
    );

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.path_tracer.enabled").ToBool()) {
        properties.Set("PATHTRACER");
    } else if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.reflections").ToBool()) {
        properties.Set("DEBUG_REFLECTIONS");
    } else if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.debug.irradiance").ToBool()) {
        properties.Set("DEBUG_IRRADIANCE");
    }

    return properties;
}

#pragma region Deferred pass

DeferredPass::DeferredPass(DeferredPassMode mode)
    : FullScreenPass(InternalFormat::RGBA16F),
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

    AddToGlobalDescriptorSet();
}

void DeferredPass::CreateShader()
{
    static const FixedArray<ShaderProperties, uint32(LightType::MAX)> light_type_properties {
        ShaderProperties { { "LIGHT_TYPE_DIRECTIONAL" } },
        ShaderProperties { { "LIGHT_TYPE_POINT" } },
        ShaderProperties { { "LIGHT_TYPE_SPOT" } },
        ShaderProperties { { "LIGHT_TYPE_AREA_RECT" } }
    };

    switch (m_mode) {
    case DeferredPassMode::INDIRECT_LIGHTING:
        m_shader = g_shader_manager->GetOrCreate(
            NAME("DeferredIndirect"),
            GetDeferredShaderProperties()
        );

        AssertThrow(m_shader.IsValid());

        break;
    case DeferredPassMode::DIRECT_LIGHTING:
        for (uint32 i = 0; i < uint32(LightType::MAX); i++) {
            ShaderProperties shader_properties = GetDeferredShaderProperties();
            shader_properties.Merge(light_type_properties[i]);

            m_direct_light_shaders[i] = g_shader_manager->GetOrCreate(NAME("DeferredDirect"), shader_properties);

            AssertThrow(m_direct_light_shaders[i].IsValid());
        }

        break;
    default:
        HYP_FAIL("Invalid deferred pass mode");
    }
}

void DeferredPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING) {
        FullScreenPass::CreatePipeline(renderable_attributes);
        return;
    }

    { // linear transform cosines texture data
        m_ltc_sampler = g_rendering_api->MakeSampler(
            renderer::FilterMode::TEXTURE_FILTER_NEAREST,
            renderer::FilterMode::TEXTURE_FILTER_LINEAR,
            renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        );

        DeferCreate(m_ltc_sampler);

        ByteBuffer ltc_matrix_data(sizeof(s_ltc_matrix), s_ltc_matrix);

        m_ltc_matrix_texture = CreateObject<Texture>(TextureData {
            TextureDesc {
                ImageType::TEXTURE_TYPE_2D,
                InternalFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                FilterMode::TEXTURE_FILTER_LINEAR,
                FilterMode::TEXTURE_FILTER_LINEAR,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
            },
            std::move(ltc_matrix_data)
        });

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
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
            },
            std::move(ltc_brdf_data)
        });

        InitObject(m_ltc_brdf_texture);

        m_ltc_brdf_texture->SetPersistentRenderResourceEnabled(true);
    }

    for (uint32 i = 0; i < uint32(LightType::MAX); i++) {
        ShaderRef &shader = m_direct_light_shaders[i];
        AssertThrow(shader.IsValid());

        renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame_index);
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
            RenderGroupFlags::NONE
        );

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_direct_light_render_groups[i] = render_group;

        if (i == 0) {
            m_render_group = render_group;
        }
    }
}

void DeferredPass::Resize_Internal(Vec2u new_size)
{
    FullScreenPass::Resize_Internal(new_size);

    AddToGlobalDescriptorSet();
}

void DeferredPass::AddToGlobalDescriptorSet()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        switch (m_mode) {
        case DeferredPassMode::INDIRECT_LIGHTING:
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DeferredIndirectResultTexture"), GetAttachment(0)->GetImageView());

            break;
        case DeferredPassMode::DIRECT_LIGHTING:
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DeferredDirectResultTexture"), GetAttachment(0)->GetImageView());

            break;
        default:
            HYP_FAIL("Invalid deferred pass mode");
        }
    }
}

void DeferredPass::Render(FrameBase *frame)
{
    HYP_SCOPE;

    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING) {
        RenderToFramebuffer(frame, nullptr);

        return;
    }

    // no lights bound, do not render direct shading at all
    if (g_engine->GetRenderState()->NumBoundLights() == 0) {
        return;
    }

    static const bool use_bindless_textures = g_rendering_api->GetRenderConfig().IsBindlessSupported();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    const EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    // render with each light
    for (uint32 light_type_index = 0; light_type_index < uint32(LightType::MAX); light_type_index++) {
        const LightType light_type = LightType(light_type_index);

        const Handle<RenderGroup> &render_group = m_direct_light_render_groups[light_type_index];

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
        const uint32 material_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
        const uint32 deferred_direct_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DeferredDirectDescriptorSet"));

        render_group->GetPipeline()->SetPushConstants(
            m_push_constant_data.Data(),
            m_push_constant_data.Size()
        );

        frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());

        frame->GetCommandList().Add<BindDescriptorSet>(
            render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame->GetFrameIndex()),
            render_group->GetPipeline(),
            ArrayMap<Name, uint32> { },
            global_descriptor_set_index
        );

        // Bind textures globally (bindless)
        if (material_descriptor_set_index != ~0u && use_bindless_textures) {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> { },
                material_descriptor_set_index
            );
        }

        if (deferred_direct_descriptor_set_index != ~0u) {
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> { },
                deferred_direct_descriptor_set_index
            );
        }

        const auto &lights = g_engine->GetRenderState()->bound_lights[uint32(light_type)];

        for (const TResourceHandle<LightRenderResource> &it : lights) {
            LightRenderResource &light_render_resource = *it;
            AssertThrow(light_render_resource.GetBufferIndex() != ~0u);

            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame->GetFrameIndex()),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid ? env_grid->GetComponentIndex() : 0) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(&light_render_resource) }
                },
                scene_descriptor_set_index
            );
            
            // Bind material descriptor set (for area lights)
            if (material_descriptor_set_index != ~0u && !use_bindless_textures && light_render_resource.GetMaterial().IsValid()) {
                const DescriptorSetRef &material_descriptor_set = light_render_resource.GetMaterial()->GetRenderResource().GetDescriptorSets()[frame->GetFrameIndex()];
                AssertThrow(material_descriptor_set != nullptr);

                frame->GetCommandList().Add<BindDescriptorSet>(
                    material_descriptor_set,
                    render_group->GetPipeline(),
                    ArrayMap<Name, uint32> { },
                    material_descriptor_set_index
                );
            }

            m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());
        }
    }
}

#pragma endregion Deferred pass

#pragma region Env grid pass

static ApplyEnvGridMode EnvGridTypeToApplyEnvGridMode(EnvGridType type)
{
    switch (type) {
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

EnvGridPass::EnvGridPass(EnvGridPassMode mode)
    : FullScreenPass(
        mode == EnvGridPassMode::RADIANCE
            ? env_grid_radiance_format
            : env_grid_irradiance_format,
        mode == EnvGridPassMode::RADIANCE
            ? env_grid_radiance_extent
            : env_grid_irradiance_extent
      ),
      m_mode(mode),
      m_is_first_frame(true)
{
    if (mode == EnvGridPassMode::RADIANCE) {
        SetBlendFunction(BlendFunction(
            BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA
        ));
    }
}

EnvGridPass::~EnvGridPass()
{
}

void EnvGridPass::Create()
{
    Threads::AssertOnThread(g_render_thread);

    FullScreenPass::Create();

    AddToGlobalDescriptorSet();
}

void EnvGridPass::CreatePipeline()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .flags          = MaterialAttributeFlags::NONE
        }
    );

    if (m_mode == EnvGridPassMode::RADIANCE) {
        m_shader = g_shader_manager->GetOrCreate(NAME("ApplyEnvGrid"), ShaderProperties { { "MODE_RADIANCE" } });

        FullScreenPass::CreatePipeline(renderable_attributes);

        return;
    }

    static const FixedArray<Pair<ApplyEnvGridMode, ShaderProperties>, uint32(ApplyEnvGridMode::MAX)> apply_env_grid_passes = {
        Pair<ApplyEnvGridMode, ShaderProperties> {
            ApplyEnvGridMode::SH,
            ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_SH" } }
        },
        Pair<ApplyEnvGridMode, ShaderProperties> {
            ApplyEnvGridMode::VOXEL,
            ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_VOXEL" } }
        },
        Pair<ApplyEnvGridMode, ShaderProperties> {
            ApplyEnvGridMode::LIGHT_FIELD,
            ShaderProperties { { "MODE_IRRADIANCE", "IRRADIANCE_MODE_LIGHT_FIELD" } }
        }
    };

    for (const auto &it : apply_env_grid_passes) {
        ShaderRef shader = g_shader_manager->GetOrCreate(NAME("ApplyEnvGrid"), it.second);
        AssertThrow(shader.IsValid());
        
        renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);
        DeferCreate(descriptor_table);

        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            shader,
            renderable_attributes,
            descriptor_table,
            RenderGroupFlags::NONE
        );

        render_group->AddFramebuffer(m_framebuffer);

        InitObject(render_group);

        m_render_groups[uint32(it.first)] = std::move(render_group);
    }

    m_render_group = m_render_groups[uint32(ApplyEnvGridMode::SH)];    
}

void EnvGridPass::AddToGlobalDescriptorSet()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        switch (m_mode) {
        case EnvGridPassMode::RADIANCE:
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("EnvGridRadianceResultTexture"), GetFinalImageView());

            break;
        case EnvGridPassMode::IRRADIANCE:
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("EnvGridIrradianceResultTexture"), GetFinalImageView());

            break;
        }
    }
}

void EnvGridPass::Resize_Internal(Vec2u new_size)
{
    FullScreenPass::Resize_Internal(new_size);

    AddToGlobalDescriptorSet();
}

void EnvGridPass::Render(FrameBase *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();
    AssertThrow(env_grid != nullptr);

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr) {
        RenderPreviousTextureToScreen(frame);
    }

    const Handle<RenderGroup> &render_group = m_mode == EnvGridPassMode::RADIANCE
        ? m_render_group
        : m_render_groups[uint32(EnvGridTypeToApplyEnvGridMode(env_grid->GetEnvGridType()))];

    AssertThrow(render_group.IsValid());

    const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const uint32 scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

    render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());

    if (ShouldRenderHalfRes()) {
        const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline(), viewport_offset, viewport_extent);
    } else {
        frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());
    }

    frame->GetCommandList().Add<BindDescriptorSet>(
        render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index),
        render_group->GetPipeline(),
        ArrayMap<Name, uint32> { },
        global_descriptor_set_index
    );

    frame->GetCommandList().Add<BindDescriptorSet>(
        render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index),
        render_group->GetPipeline(),
        ArrayMap<Name, uint32> {
            { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid->GetComponentIndex()) }
        },
        scene_descriptor_set_index
    );

    m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frame_index);

    if (ShouldRenderHalfRes()) {
        MergeHalfResTextures(frame);
    }

    if (UsesTemporalBlending()) {
        if (!ShouldRenderHalfRes()) {
            CopyResultToPreviousTexture(frame);
        }

        m_temporal_blending->Render(frame);
    }

    m_is_first_frame = false;
}

#pragma endregion Env grid pass

#pragma region ReflectionsPass

ReflectionsPass::ReflectionsPass()
    : FullScreenPass(InternalFormat::R10G10B10A2),
      m_is_first_frame(true)
{
    SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA
    ));
}

ReflectionsPass::~ReflectionsPass()
{
    m_ssr_renderer->Destroy();
    m_ssr_renderer.Reset();
}

void ReflectionsPass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreateSSRRenderer();

    AddToGlobalDescriptorSet();
}

void ReflectionsPass::CreatePipeline()
{
    HYP_SCOPE;

    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                            BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .flags          = MaterialAttributeFlags::NONE
        }
    ));
}

void ReflectionsPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // Default pass type (non parallax corrected)

    static const FixedArray<Pair<ApplyReflectionProbeMode, ShaderProperties>, ApplyReflectionProbeMode::MAX> apply_reflection_probe_passes = {
        Pair<ApplyReflectionProbeMode, ShaderProperties> {
            ApplyReflectionProbeMode::DEFAULT,
            ShaderProperties { }
        },
        Pair<ApplyReflectionProbeMode, ShaderProperties> {
            ApplyReflectionProbeMode::PARALLAX_CORRECTED,
            ShaderProperties { { "ENV_PROBE_PARALLAX_CORRECTED" } }
        }
    };

    for (const auto &it : apply_reflection_probe_passes) {
        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("ApplyReflectionProbe"),
            it.second
        );

        AssertThrow(shader.IsValid());
        
        renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);
        DeferCreate(descriptor_table);

        Handle<RenderGroup> render_group = CreateObject<RenderGroup>(
            shader,
            renderable_attributes,
            descriptor_table,
            RenderGroupFlags::NONE
        );

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
    m_ssr_renderer = MakeUnique<SSRRenderer>(SSRRendererConfig::FromConfig());
    m_ssr_renderer->Create();
    
    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), m_ssr_renderer->GetFinalResultTexture()->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptor_table);

    m_render_ssr_to_screen_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
    );

    // Use alpha blending to blend SSR into the reflection probes
    m_render_ssr_to_screen_pass->SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA
    ));

    m_render_ssr_to_screen_pass->Create();
}

void ReflectionsPass::AddToGlobalDescriptorSet()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("ReflectionProbeResultTexture"), GetFinalImageView());
    }
}

void ReflectionsPass::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(new_size);

    AddToGlobalDescriptorSet();
}

void ReflectionsPass::Render(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    if (ShouldRenderSSR()) { // screen space reflection
        m_ssr_renderer->Render(frame);
    }

    // Sky renders first
    static const FixedArray<EnvProbeType, ApplyReflectionProbeMode::MAX> reflection_probe_types {
        ENV_PROBE_TYPE_SKY,
        ENV_PROBE_TYPE_REFLECTION
    };

    static const FixedArray<ApplyReflectionProbeMode, ApplyReflectionProbeMode::MAX> reflection_probe_modes {
        ApplyReflectionProbeMode::DEFAULT,              // ENV_PROBE_TYPE_SKY
        ApplyReflectionProbeMode::PARALLAX_CORRECTED    // ENV_PROBE_TYPE_REFLECTION
    };

    FixedArray<Pair<Handle<RenderGroup> *, Array<EnvProbeRenderResource *>>, ApplyReflectionProbeMode::MAX> pass_ptrs;

    for (uint32 mode_index = ApplyReflectionProbeMode::DEFAULT; mode_index < ApplyReflectionProbeMode::MAX; mode_index++) {
        pass_ptrs[mode_index] = {
            &m_render_groups[mode_index],
            { }
        };

        const EnvProbeType env_probe_type = reflection_probe_types[mode_index];

        for (const TResourceHandle<EnvProbeRenderResource> &resource_handle : g_engine->GetRenderState()->bound_env_probes[env_probe_type]) {
            pass_ptrs[mode_index].second.PushBack(resource_handle.Get());
        }
    }

    frame->GetCommandList().Add<BeginFramebuffer>(GetFramebuffer(), frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr) {
        RenderPreviousTextureToScreen(frame);
    }
    
    uint32 num_rendered_env_probes = 0;

    for (uint32 reflection_probe_type_index = 0; reflection_probe_type_index < ArraySize(reflection_probe_types); reflection_probe_type_index++) {
        const EnvProbeType env_probe_type = reflection_probe_types[reflection_probe_type_index];
        const ApplyReflectionProbeMode mode = reflection_probe_modes[reflection_probe_type_index];

        const Pair<Handle<RenderGroup> *, Array<EnvProbeRenderResource *>> &it = pass_ptrs[mode];

        if (it.second.Empty()) {
            continue;
        }

        const Handle<RenderGroup> &render_group = *it.first;
        const Array<EnvProbeRenderResource *> &env_probe_render_resources = it.second;

        render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());

        if (ShouldRenderHalfRes()) {
            const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
            const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline(), viewport_offset, viewport_extent);
        } else {
            frame->GetCommandList().Add<BindGraphicsPipeline>(render_group->GetPipeline());
        }

        const uint32 global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
        const uint32 scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

        frame->GetCommandList().Add<BindDescriptorSet>(
            render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index),
            render_group->GetPipeline(),
            ArrayMap<Name, uint32> { },
            global_descriptor_set_index
        );

        for (EnvProbeRenderResource *env_probe_render_resource : env_probe_render_resources) {
            if (num_rendered_env_probes >= max_bound_reflection_probes) {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }
            frame->GetCommandList().Add<BindDescriptorSet>(
                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index),
                render_group->GetPipeline(),
                ArrayMap<Name, uint32> {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource->GetBufferIndex()) }
                },
                scene_descriptor_set_index
            );

            m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

            ++num_rendered_env_probes;
        }
    }

    if (ShouldRenderSSR()) {
        m_render_ssr_to_screen_pass->RenderToFramebuffer(frame, GetFramebuffer());
    }

    frame->GetCommandList().Add<EndFramebuffer>(GetFramebuffer(), frame_index);

    if (ShouldRenderHalfRes()) {
        MergeHalfResTextures(frame);
    }

    if (UsesTemporalBlending()) {
        if (!ShouldRenderHalfRes()) {
            CopyResultToPreviousTexture(frame);
        }

        m_temporal_blending->Render(frame);
    }

    m_is_first_frame = false;
}

#pragma endregion ReflectionsPass

#pragma region Deferred renderer

DeferredRenderer::DeferredRenderer(SwapchainBase *swapchain)
    : m_gbuffer(MakeUnique<GBuffer>(swapchain)),
      m_is_initialized(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(!m_is_initialized);

    { // initialize GBuffer
        m_gbuffer->Create();

        static FixedArray<Name, GBUFFER_RESOURCE_MAX> gbuffer_texture_names {
            NAME("GBufferAlbedoTexture"),
            NAME("GBufferNormalsTexture"),
            NAME("GBufferMaterialTexture"),
            NAME("GBufferLightmapTexture"),
            NAME("GBufferVelocityTexture"),
            NAME("GBufferMaskTexture"),
            NAME("GBufferWSNormalsTexture"),
            NAME("GBufferTranslucentTexture")
        };

        auto InitializeGBufferFramebuffers = [this]()
        {
            HYP_SCOPE;
            Threads::AssertOnThread(g_render_thread);

            m_opaque_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetFramebuffer();
            m_translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();

            // depth attachment goes into separate slot
            AttachmentBase *depth_attachment = m_opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
            AssertThrow(depth_attachment != nullptr);

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                uint32 element_index = 0;

                if (g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported()) {

                    // not including depth texture here (hence the - 1)
                    for (uint32 attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                            ->SetElement(NAME("GBufferTextures"), element_index++, m_opaque_fbo->GetAttachment(attachment_index)->GetImageView());
                    }

                    // add translucent bucket's albedo
                    g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                        ->SetElement(NAME("GBufferTextures"), element_index++, m_translucent_fbo->GetAttachment(0)->GetImageView());
                } else {
                    for (uint32 attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                            ->SetElement(gbuffer_texture_names[attachment_index], m_opaque_fbo->GetAttachment(attachment_index)->GetImageView());
                    }

                    // add translucent bucket's albedo
                    g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                        ->SetElement(NAME("GBufferTranslucentTexture"), m_translucent_fbo->GetAttachment(0)->GetImageView());
                }

                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                    ->SetElement(NAME("GBufferDepthTexture"), depth_attachment->GetImageView());
            }
        };

        InitializeGBufferFramebuffers();

        m_gbuffer->OnGBufferResolutionChanged.Bind([this, InitializeGBufferFramebuffers](...)
        {
            InitializeGBufferFramebuffers();
        }, g_render_thread).Detach();
    }

    m_env_grid_radiance_pass = MakeUnique<EnvGridPass>(EnvGridPassMode::RADIANCE);
    m_env_grid_radiance_pass->Create();

    m_env_grid_irradiance_pass = MakeUnique<EnvGridPass>(EnvGridPassMode::IRRADIANCE);
    m_env_grid_irradiance_pass->Create();

    m_reflections_pass = MakeUnique<ReflectionsPass>();
    m_reflections_pass->Create();

    m_ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig());
    m_ssgi->Create();

    m_post_processing.Create();

    m_indirect_pass = MakeUnique<DeferredPass>(DeferredPassMode::INDIRECT_LIGHTING);
    m_indirect_pass->Create();

    m_direct_pass = MakeUnique<DeferredPass>(DeferredPassMode::DIRECT_LIGHTING);
    m_direct_pass->Create();

    m_depth_pyramid_renderer = MakeUnique<DepthPyramidRenderer>();
    m_depth_pyramid_renderer->Create();

    m_mip_chain = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        mip_chain_format,
        Vec3u { mip_chain_extent.x, mip_chain_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    InitObject(m_mip_chain);

    m_mip_chain->SetPersistentRenderResourceEnabled(true);

    m_hbao = MakeUnique<HBAO>(HBAOConfig::FromConfig());
    m_hbao->Create();

    CreateBlueNoiseBuffer();
    CreateSphereSamplesBuffer();

    // m_dof_blur = MakeUnique<DOFBlur>(m_gbuffer->GetResolution());
    // m_dof_blur->Create();

    CreateCombinePass();
    CreateDescriptorSets();

    m_temporal_aa = MakeUnique<TemporalAA>(m_gbuffer->GetResolution());
    m_temporal_aa->Create();

    m_is_initialized = true;
}

void DeferredRenderer::CreateDescriptorSets()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
    
    // set global gbuffer data
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("GBufferMipChain"), m_mip_chain->GetRenderResource().GetImageView());
    }
}

void DeferredRenderer::CreateCombinePass()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
    
    ShaderRef shader = g_shader_manager->GetOrCreate(
        NAME("DeferredCombine"),
        GetDeferredShaderProperties()
    );

    AssertThrow(shader.IsValid());

    m_combine_pass = MakeUnique<FullScreenPass>(shader, InternalFormat::R11G11B10F);
    m_combine_pass->Create();

    PUSH_RENDER_COMMAND(
        SetDeferredResultInGlobalDescriptorSet,
        GetCombinedResultAttachment()->GetImageView()
    );
}

void DeferredRenderer::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    m_depth_pyramid_renderer.Reset();

    m_hbao.Reset();

    m_temporal_aa.Reset();

    // m_dof_blur->Destroy();

    m_ssgi->Destroy();
    m_ssgi.Reset();

    m_post_processing.Destroy();

    m_combine_pass.Reset();

    m_env_grid_radiance_pass.Reset();
    m_env_grid_irradiance_pass.Reset();

    m_reflections_pass.Reset();

    m_mip_chain.Reset();

    m_opaque_fbo.Reset();
    m_translucent_fbo.Reset();

    m_indirect_pass.Reset();
    m_direct_pass.Reset();

    m_gbuffer->Destroy();
}

void DeferredRenderer::Resize(Vec2u new_size)
{
    HYP_SCOPE;
    
    Threads::AssertOnThread(g_render_thread);

    if (!m_is_initialized) {
        return;
    }

    m_gbuffer->Resize(new_size);

    m_direct_pass->Resize(new_size);
    m_indirect_pass->Resize(new_size);

    m_combine_pass->Resize(new_size);

    m_env_grid_radiance_pass->Resize(new_size);
    m_env_grid_irradiance_pass->Resize(new_size);

    m_reflections_pass->Resize(new_size);
}

void DeferredRenderer::Render(FrameBase *frame, RenderEnvironment *environment)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    static const bool is_raytracing_supported = g_rendering_api->GetRenderConfig().IsRaytracingSupported();

    const uint32 frame_index = frame->GetFrameIndex();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();
    const TResourceHandle<EnvProbeRenderResource> &env_probe_render_resource = g_engine->GetRenderState()->GetActiveEnvProbe();
    EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    const bool is_render_environment_ready = environment && environment->IsReady();

    const bool do_particles = true;
    const bool do_gaussian_splatting = false;//environment && environment->IsReady();

    const bool use_rt_radiance = is_raytracing_supported
        && (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.path_tracer.enabled").ToBool()
            || g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool());

    const bool use_ddgi = is_raytracing_supported
        && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool();
    
    const bool use_hbao = g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbao.enabled").ToBool();
    const bool use_hbil = g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool();

    const bool use_ssgi = g_engine->GetAppContext()->GetConfiguration().Get("rendering.ssgi.enabled").ToBool();

    const bool use_env_grid_irradiance = env_grid != nullptr && g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.enabled").ToBool();
    const bool use_env_grid_radiance = env_grid != nullptr && g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool();

    const bool use_reflection_probes = g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()
        || g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any();

    const bool use_temporal_aa = g_engine->GetAppContext()->GetConfiguration().Get("rendering.taa.enabled").ToBool() && m_temporal_aa != nullptr;

    if (use_temporal_aa) {
        ApplyCameraJitter();
    }

    struct alignas(128)
    {
        uint32  flags;
        uint32  screen_width;
        uint32  screen_height;
    } deferred_data;

    Memory::MemSet(&deferred_data, 0, sizeof(deferred_data));

    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferred_data.flags |= use_ddgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;

    deferred_data.screen_width = m_gbuffer->GetResolution().x;
    deferred_data.screen_height = m_gbuffer->GetResolution().y;

    CollectDrawCalls(frame);
    
    if (is_render_environment_ready) {
        if (do_particles) {
            environment->GetParticleSystem()->UpdateParticles(frame);
        }

        if (do_gaussian_splatting) {
            environment->GetGaussianSplatting()->UpdateSplats(frame);
        }
    }

    m_indirect_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
    m_direct_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));

#if 1
    { // opaque objects
        frame->GetCommandList().Add<BeginFramebuffer>(m_opaque_fbo, frame_index);

        RenderOpaqueObjects(frame);

        frame->GetCommandList().Add<EndFramebuffer>(m_opaque_fbo, frame_index);
    }
    // end opaque objs

    if (use_env_grid_irradiance) { // submit env grid command buffer
        m_env_grid_irradiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_irradiance_pass->Render(frame);
    }

    if (use_env_grid_radiance) { // submit env grid command buffer
        m_env_grid_radiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_radiance_pass->Render(frame);
    }

    if (use_reflection_probes) {
        m_reflections_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_reflections_pass->Render(frame);
    }

    if (is_render_environment_ready) {
        if (use_rt_radiance) {
            environment->RenderRTRadiance(frame);
        }

        if (use_ddgi) {
            environment->RenderDDGIProbes(frame);
        }
    }

    if (use_hbao || use_hbil) {
        m_hbao->Render(frame);
    }

    if (use_ssgi) {
        bool is_sky_set = false;

        // Bind sky env probe for SSGI.
        if (g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
            g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Front()));

            is_sky_set = true;
        }

        m_ssgi->Render(frame);

        if (is_sky_set) {
            g_engine->GetRenderState()->UnsetActiveEnvProbe();
        }
    }

    m_post_processing.RenderPre(frame);

    // render indirect and direct lighting into the same framebuffer
    const FramebufferRef &deferred_pass_framebuffer = m_indirect_pass->GetFramebuffer();

    { // deferred lighting on opaque objects
        frame->GetCommandList().Add<BeginFramebuffer>(deferred_pass_framebuffer, frame_index);

        m_indirect_pass->Render(frame);

        if (g_engine->GetRenderState()->NumBoundLights() != 0) {
            m_direct_pass->Render(frame);
        }

        frame->GetCommandList().Add<EndFramebuffer>(deferred_pass_framebuffer, frame_index);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const ImageRef &src_image = deferred_pass_framebuffer->GetAttachment(0)->GetImage();
        GenerateMipChain(frame, src_image);
    }
#endif

    { // translucent objects
        frame->GetCommandList().Add<BeginFramebuffer>(m_translucent_fbo, frame_index);

        bool is_sky_set = false;

        // Bind sky env probe
        if (g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
            g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Front()));

            is_sky_set = true;
        }

        // begin translucent with forward rendering
        RenderTranslucentObjects(frame);

        if (is_render_environment_ready) {
            if (do_particles) {
                environment->GetParticleSystem()->Render(frame);
            }

            if (do_gaussian_splatting) {
                environment->GetGaussianSplatting()->Render(frame);
            }
        }

        if (is_sky_set) {
            g_engine->GetRenderState()->UnsetActiveEnvProbe();
        }

        RenderSkybox(frame);

        // render debug draw
        g_engine->GetDebugDrawer()->Render(frame);

        frame->GetCommandList().Add<EndFramebuffer>(m_translucent_fbo, frame_index);
    }

    {
        struct alignas(128)
        {
            Vec2u   image_dimensions;
            uint32  _pad0, _pad1;
            uint32  deferred_flags;
        } deferred_combine_constants;

        deferred_combine_constants.image_dimensions = m_combine_pass->GetFramebuffer()->GetExtent();
        deferred_combine_constants.deferred_flags = deferred_data.flags;

        m_combine_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&deferred_combine_constants, sizeof(deferred_combine_constants));
        m_combine_pass->Begin(frame);

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_combine_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
            m_combine_pass->GetRenderGroup()->GetPipeline(),
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) },
                        { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid ? env_grid->GetComponentIndex() : 0) },
                        { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                    }
                }
            },
            frame_index
        );

        m_combine_pass->GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
        m_combine_pass->End(frame);
    }

    { // render depth pyramid
        m_depth_pyramid_renderer->Render(frame);
        // update culling info now that depth pyramid has been rendered
        m_cull_data.depth_pyramid_image_view = m_depth_pyramid_renderer->GetResultImageView();
        m_cull_data.depth_pyramid_dimensions = m_depth_pyramid_renderer->GetExtent();
    }

    m_post_processing.RenderPost(frame);

    if (use_temporal_aa) {
        m_temporal_aa->Render(frame);
    }

    // depth of field
    // m_dof_blur->Render(frame);
}

void DeferredRenderer::GenerateMipChain(FrameBase *frame, const ImageRef &src_image)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const ImageRef &mipmapped_result = m_mip_chain->GetRenderResource().GetImage();
    AssertThrow(mipmapped_result.IsValid());

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(mipmapped_result, renderer::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    frame->GetCommandList().Add<BlitRect>(
        src_image,
        mipmapped_result,
        Rect<uint32> { 0, 0, src_image->GetExtent().x, src_image->GetExtent().y },
        Rect<uint32> { 0, 0, mipmapped_result->GetExtent().x, mipmapped_result->GetExtent().y }
    );

    frame->GetCommandList().Add<GenerateMipmaps>(mipmapped_result);

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::SHADER_RESOURCE);
}

// @TODO Move to CameraRenderResource
void DeferredRenderer::ApplyCameraJitter()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    static const float jitter_scale = 0.25f;

    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    const uint32 camera_buffer_index = camera_resource_handle->GetBufferIndex();
    AssertThrow(camera_buffer_index != ~0u);

    Vec4f jitter = Vec4f::Zero();

    const uint32 frame_counter = g_engine->GetRenderState()->frame_counter + 1;

    CameraShaderData &buffer_data = g_engine->GetRenderData()->cameras->Get<CameraShaderData>(camera_buffer_index);

    if (buffer_data.projection[3][3] < MathUtil::epsilon_f) {
        Matrix4::Jitter(frame_counter, buffer_data.dimensions.x, buffer_data.dimensions.y, jitter);

        buffer_data.jitter = jitter * jitter_scale;

        g_engine->GetRenderData()->cameras->MarkDirty(camera_buffer_index);
    }
}

void DeferredRenderer::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    static_assert(sizeof(BlueNoiseBuffer::sobol_256spp_256d) == sizeof(BlueNoise::sobol_256spp_256d));
    static_assert(sizeof(BlueNoiseBuffer::scrambling_tile) == sizeof(BlueNoise::scrambling_tile));
    static_assert(sizeof(BlueNoiseBuffer::ranking_tile) == sizeof(BlueNoise::ranking_tile));

    constexpr SizeType blue_noise_buffer_size = sizeof(BlueNoiseBuffer);

    constexpr SizeType sobol_256spp_256d_offset = offsetof(BlueNoiseBuffer, sobol_256spp_256d);
    constexpr SizeType sobol_256spp_256d_size = sizeof(BlueNoise::sobol_256spp_256d);
    constexpr SizeType scrambling_tile_offset = offsetof(BlueNoiseBuffer, scrambling_tile);
    constexpr SizeType scrambling_tile_size = sizeof(BlueNoise::scrambling_tile);
    constexpr SizeType ranking_tile_offset = offsetof(BlueNoiseBuffer, ranking_tile);
    constexpr SizeType ranking_tile_size = sizeof(BlueNoise::ranking_tile);

    static_assert(blue_noise_buffer_size == (sobol_256spp_256d_offset + sobol_256spp_256d_size)
        + ((scrambling_tile_offset - (sobol_256spp_256d_offset + sobol_256spp_256d_size)) + scrambling_tile_size)
        + ((ranking_tile_offset - (scrambling_tile_offset + scrambling_tile_size)) + ranking_tile_size));
    
    GPUBufferRef blue_noise_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STORAGE_BUFFER, sizeof(BlueNoiseBuffer));
    HYPERION_ASSERT_RESULT(blue_noise_buffer->Create());
    blue_noise_buffer->Copy(sobol_256spp_256d_offset, sobol_256spp_256d_size, &BlueNoise::sobol_256spp_256d[0]);
    blue_noise_buffer->Copy(scrambling_tile_offset, scrambling_tile_size, &BlueNoise::scrambling_tile[0]);
    blue_noise_buffer->Copy(ranking_tile_offset, ranking_tile_size, &BlueNoise::ranking_tile[0]);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("BlueNoiseBuffer"), blue_noise_buffer);
    }
}

void DeferredRenderer::CreateSphereSamplesBuffer()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);
    
    GPUBufferRef sphere_samples_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(Vec4f) * 4096);
    HYPERION_ASSERT_RESULT(sphere_samples_buffer->Create());

    Vec4f *sphere_samples = new Vec4f[4096];

    uint32 seed = 0;

    for (uint32 i = 0; i < 4096; i++) {
        Vec3f sample = MathUtil::RandomInSphere(Vec3f {
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed),
            MathUtil::RandomFloat(seed)
        });

        sphere_samples[i] = Vec4f(sample, 0.0f);
    }

    sphere_samples_buffer->Copy(sizeof(Vec4f) * 4096, sphere_samples);

    delete[] sphere_samples;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("SphereSamplesBuffer"), sphere_samples_buffer);
    }
}

void DeferredRenderer::CollectDrawCalls(FrameBase *frame)
{
    HYP_SCOPE;

    const WorldRenderResource &world_render_resource = g_engine->GetWorld()->GetRenderResource();

    for (const TResourceHandle<SceneRenderResource> &scene_resource_handle : world_render_resource.GetBoundScenes()) {
        scene_resource_handle->GetScene()->GetRenderCollector().CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_SKYBOX) | (1 << BUCKET_TRANSLUCENT)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderSkybox(FrameBase *frame)
{
    HYP_SCOPE;

    const WorldRenderResource &world_render_resource = g_engine->GetWorld()->GetRenderResource();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    for (const TResourceHandle<SceneRenderResource> &scene_resource_handle : world_render_resource.GetBoundScenes()) {
        scene_resource_handle->GetScene()->GetRenderCollector().ExecuteDrawCalls(
            frame,
            camera_resource_handle,
            nullptr,
            Bitset((1 << BUCKET_SKYBOX)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderOpaqueObjects(FrameBase *frame)
{
    HYP_SCOPE;

    const WorldRenderResource &world_render_resource = g_engine->GetWorld()->GetRenderResource();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    for (const TResourceHandle<SceneRenderResource> &scene_resource_handle : world_render_resource.GetBoundScenes()) {
        scene_resource_handle->GetScene()->GetRenderCollector().ExecuteDrawCalls(
            frame,
            camera_resource_handle,
            nullptr,
            Bitset((1 << BUCKET_OPAQUE)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderTranslucentObjects(FrameBase *frame)
{
    HYP_SCOPE;

    const WorldRenderResource &world_render_resource = g_engine->GetWorld()->GetRenderResource();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    for (const TResourceHandle<SceneRenderResource> &scene_resource_handle : world_render_resource.GetBoundScenes()) {
        scene_resource_handle->GetScene()->GetRenderCollector().ExecuteDrawCalls(
            frame,
            camera_resource_handle,
            nullptr,
            Bitset((1 << BUCKET_TRANSLUCENT)),
            &m_cull_data
        );
    }
}

#pragma endregion Deferred pass

} // namespace hyperion
