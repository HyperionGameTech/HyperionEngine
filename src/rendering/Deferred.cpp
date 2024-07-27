/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Deferred.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/system/AppContext.hpp>

#include <util/BlueNoise.hpp>

#include <util/Float16.hpp>
#include <util/fs/FsUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Result;
using renderer::GPUBufferType;

static const Extent2D mip_chain_extent { 512, 512 };
static const InternalFormat mip_chain_format = InternalFormat::R10G10B10A2;

static const Extent2D hbao_extent { 512, 512 };
static const Extent2D ssr_extent { 512, 512 };

static const InternalFormat env_grid_radiance_format = InternalFormat::RGBA8_SRGB;
static const InternalFormat env_grid_irradiance_format = InternalFormat::R11G11B10F;
static const Extent2D env_grid_irradiance_extent { 1024, 768 };
static const Extent2D env_grid_radiance_extent { 1024, 768 };

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

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("DeferredResult"), result_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateBlueNoiseBuffer) : renderer::RenderCommand
{
    GPUBufferRef buffer;

    RENDER_COMMAND(CreateBlueNoiseBuffer)(const GPUBufferRef &buffer)
        : buffer(buffer)
    {
    }

    virtual Result operator()()
    {
        AssertThrow(buffer.IsValid());

        static_assert(sizeof(BlueNoiseBuffer::sobol_256spp_256d) == sizeof(BlueNoise::sobol_256spp_256d));
        static_assert(sizeof(BlueNoiseBuffer::scrambling_tile) == sizeof(BlueNoise::scrambling_tile));
        static_assert(sizeof(BlueNoiseBuffer::ranking_tile) == sizeof(BlueNoise::ranking_tile));

        UniquePtr<BlueNoiseBuffer> aligned_buffer(new BlueNoiseBuffer);
        Memory::MemCpy(&aligned_buffer->sobol_256spp_256d[0], BlueNoise::sobol_256spp_256d, sizeof(BlueNoise::sobol_256spp_256d));
        Memory::MemCpy(&aligned_buffer->scrambling_tile[0], BlueNoise::scrambling_tile, sizeof(BlueNoise::scrambling_tile));
        Memory::MemCpy(&aligned_buffer->ranking_tile[0], BlueNoise::ranking_tile, sizeof(BlueNoise::ranking_tile));

        HYPERION_BUBBLE_ERRORS(buffer->Create(g_engine->GetGPUDevice(), sizeof(BlueNoiseBuffer)));

        buffer->Copy(g_engine->GetGPUDevice(), sizeof(BlueNoiseBuffer), aligned_buffer.Get());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

static ShaderProperties GetDeferredShaderProperties()
{
    ShaderProperties properties;

    properties.Set(
        "RT_REFLECTIONS_ENABLED",
        g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()
            && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool()
    );

    properties.Set(
        "RT_GI_ENABLED",
        g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()
            && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool()
    );

    properties.Set(
        "SSR_ENABLED",
        !g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool()
            && g_engine->GetAppContext()->GetConfiguration().Get("rendering.ssr.enabled").ToBool()
    );

    properties.Set("REFLECTION_PROBE_ENABLED", true);
    
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

    properties.Set(
        "LIGHT_RAYS_ENABLED",
        g_engine->GetAppContext()->GetConfiguration().Get("rendering.light_rays.enabled").ToBool()
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
    : FullScreenPass(InternalFormat::RGBA8_SRGB),
      m_mode(mode)
{
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
    static const FixedArray<ShaderProperties, uint(LightType::MAX)> light_type_properties {
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
        for (uint i = 0; i < uint(LightType::MAX); i++) {
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

void DeferredPass::CreatePipeline()
{
    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = m_mode == DeferredPassMode::DIRECT_LIGHTING
                ? BlendFunction::Additive()
                : BlendFunction::None()
        }
    ));
}

void DeferredPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING) {
        FullScreenPass::CreatePipeline(renderable_attributes);
        return;
    }

    { // linear transform cosines texture data
        m_ltc_sampler = MakeRenderObject<Sampler>(
            renderer::FilterMode::TEXTURE_FILTER_NEAREST,
            renderer::FilterMode::TEXTURE_FILTER_LINEAR,
            renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        );

        DeferCreate(m_ltc_sampler, g_engine->GetGPUDevice());

        ByteBuffer ltc_matrix_data(sizeof(s_ltc_matrix), s_ltc_matrix);
        RC<StreamedTextureData> streamed_matrix_data(new StreamedTextureData(
            TextureData {
                TextureDesc {
                    ImageType::TEXTURE_TYPE_2D,
                    InternalFormat::RGBA16F,
                    Extent3D { 64, 64, 1 },
                    FilterMode::TEXTURE_FILTER_LINEAR,
                    FilterMode::TEXTURE_FILTER_LINEAR,
                    WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
                },
                std::move(ltc_matrix_data)
            }
        ));

        m_ltc_matrix_texture = CreateObject<Texture>(std::move(streamed_matrix_data));

        InitObject(m_ltc_matrix_texture);

        ByteBuffer ltc_brdf_data(sizeof(s_ltc_brdf), s_ltc_brdf);
        RC<StreamedTextureData> streamed_brdf_data(new StreamedTextureData(
            TextureData {
                TextureDesc {
                    ImageType::TEXTURE_TYPE_2D,
                    InternalFormat::RGBA16F,
                    Extent3D { 64, 64, 1 },
                    FilterMode::TEXTURE_FILTER_LINEAR,
                    FilterMode::TEXTURE_FILTER_LINEAR,
                    WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
                },
                std::move(ltc_brdf_data)
            }
        ));

        m_ltc_brdf_texture = CreateObject<Texture>(std::move(streamed_brdf_data));

        InitObject(m_ltc_brdf_texture);
    }

    for (uint i = 0; i < uint(LightType::MAX); i++) {
        ShaderRef &shader = m_direct_light_shaders[i];
        AssertThrow(shader.IsValid());

        renderer::DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame_index);
            AssertThrow(descriptor_set.IsValid());
            
            descriptor_set->SetElement(NAME("MaterialsBuffer"), g_engine->GetRenderData()->materials.GetBuffer(frame_index));
            
            descriptor_set->SetElement(NAME("LTCSampler"), m_ltc_sampler);
            descriptor_set->SetElement(NAME("LTCMatrixTexture"), m_ltc_matrix_texture->GetImageView());
            descriptor_set->SetElement(NAME("LTCBRDFTexture"), m_ltc_brdf_texture->GetImageView());
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

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

void DeferredPass::Resize_Internal(Extent2D new_size)
{
    FullScreenPass::Resize_Internal(new_size);

    AddToGlobalDescriptorSet();
}

void DeferredPass::AddToGlobalDescriptorSet()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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

void DeferredPass::Record(uint frame_index)
{
    HYP_SCOPE;

    if (m_mode == DeferredPassMode::INDIRECT_LIGHTING) {
        FullScreenPass::Record(frame_index);

        return;
    }

    // no lights bound, do not render direct shading at all
    if (g_engine->GetRenderState().lights.Empty()) {
        return;
    }

    static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    const uint camera_index = g_engine->GetRenderState().GetCamera().id.ToIndex();

    FixedArray<Array<decltype(g_engine->GetRenderState().lights)::Iterator>, uint(LightType::MAX)> light_iterators;

    // Set up light iterators
    for (auto &it : g_engine->GetRenderState().lights) {
        const LightDrawProxy &light = it.second;

        if (light.visibility_bits & (1ull << uint64(camera_index))) {
            light_iterators[uint(light.type)].PushBack(&it);
        }
    }
    
    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetRenderPass(),
        [&](CommandBuffer *cmd)
        {
            const uint scene_index = g_engine->GetRenderState().GetScene().id.ToIndex();
            const uint env_grid_index = g_engine->GetRenderState().bound_env_grid.ToIndex();

            // render with each light
            for (uint light_type_index = 0; light_type_index < uint(LightType::MAX); light_type_index++) {
                const LightType light_type = LightType(light_type_index);

                const Handle<RenderGroup> &render_group = m_direct_light_render_groups[light_type_index];

                const uint global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
                const uint scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
                const uint material_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
                const uint deferred_direct_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("DeferredDirectDescriptorSet"));

                render_group->GetPipeline()->SetPushConstants(
                    m_push_constant_data.Data(),
                    m_push_constant_data.Size()
                );

                render_group->GetPipeline()->Bind(cmd);

                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                    ->Bind(
                        cmd,
                        render_group->GetPipeline(),
                        global_descriptor_set_index
                    );

                // Bind textures globally (bindless)
                if (material_descriptor_set_index != ~0u && use_bindless_textures) {
                    render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
                        ->Bind(
                            cmd,
                            render_group->GetPipeline(),
                            material_descriptor_set_index
                        );
                }

                if (deferred_direct_descriptor_set_index != ~0u) {
                    render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("DeferredDirectDescriptorSet"), frame_index)
                        ->Bind(
                            cmd,
                            render_group->GetPipeline(),
                            deferred_direct_descriptor_set_index
                        );
                }

                const auto &light_it = light_iterators[light_type_index];

                for (const auto &it : light_it) {
                    const ID<Light> light_id = it->first;
                    const LightDrawProxy &light = it->second;

                    // We'll use the EnvProbe slot to bind whatever EnvProbe
                    // is used for the light's shadow map (if applicable)

                    uint shadow_probe_index = 0;

                    if (light.shadow_map_index != ~0u && light_type == LightType::POINT) {
                        shadow_probe_index = light.shadow_map_index;
                    }

                    render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                        ->Bind(
                            cmd,
                            render_group->GetPipeline(),
                            {
                                { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                                { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                                { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, light_id.ToIndex()) },
                                { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, env_grid_index) },
                                { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, shadow_probe_index) }
                            },
                            scene_descriptor_set_index
                        );
                    
                    // Bind material descriptor set (for area lights)
                    if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
                        g_engine->GetMaterialDescriptorSetManager().GetDescriptorSet(light.material_id, frame_index)
                            ->Bind(cmd, render_group->GetPipeline(), material_descriptor_set_index);
                    }

                    m_full_screen_quad->Render(cmd);
                }
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void DeferredPass::Render(Frame *frame)
{
    HYP_SCOPE;

    FullScreenPass::Render(frame);
}

#pragma endregion Deferred pass

#pragma region Env grid pass

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
    m_render_texture_to_screen_pass.Reset();

    m_temporal_blending.Reset();
}

void EnvGridPass::Create()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    CreateShader();

    FullScreenPass::Create();

    if (m_mode == EnvGridPassMode::RADIANCE) {
        CreateTemporalBlending();
    }

    CreatePreviousTexture();
    CreateRenderTextureToScreenPass();

    AddToGlobalDescriptorSet();
}

void EnvGridPass::CreateShader()
{
    ShaderProperties properties { };

    switch (m_mode) {
    case EnvGridPassMode::RADIANCE:
        properties.Set("MODE_RADIANCE");

        break;
    case EnvGridPassMode::IRRADIANCE:
        switch (g_engine->GetConfig().Get(CONFIG_ENV_GRID_GI_MODE).GetInt()) {
        case 1:
            properties.Set("MODE_IRRADIANCE_VOXEL");
            break;
        case 0: // fallthrough
        default:
            properties.Set("MODE_IRRADIANCE");
            break;
        }

        break;
    }

    m_shader = g_shader_manager->GetOrCreate(
        NAME("ApplyEnvGrid"),
        properties
    );
}

void EnvGridPass::CreatePipeline()
{
    FullScreenPass::CreatePipeline(RenderableAttributeSet(
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

void EnvGridPass::CreatePreviousTexture()
{
    // Create previous image
    m_previous_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        m_image_format,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    ));

    InitObject(m_previous_texture);
}

void EnvGridPass::CreateRenderTextureToScreenPass()
{
    AssertThrow(m_previous_texture.IsValid());
    AssertThrow(m_previous_texture->GetImageView().IsValid());

    // Create render texture to screen pass.
    // this is used to render the previous frame's result to the screen,
    // so we can blend it with the current frame's result (checkerboarded)
    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), m_previous_texture->GetImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_texture_to_screen_pass.Reset(new FullScreenPass(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
    ));

    m_render_texture_to_screen_pass->Create();
}

void EnvGridPass::CreateTemporalBlending()
{
    m_temporal_blending.Reset(new TemporalBlending(
        m_framebuffer->GetExtent(),
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_1,
        TemporalBlendFeedback::LOW,
        m_framebuffer
    ));

    m_temporal_blending->Create();
}

void EnvGridPass::AddToGlobalDescriptorSet()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        switch (m_mode) {
        case EnvGridPassMode::RADIANCE:
            AssertThrow(m_temporal_blending->GetResultTexture()->GetImageView().IsValid());
            AssertThrow(m_temporal_blending->GetResultTexture()->GetImageView()->IsCreated());
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("EnvGridRadianceResultTexture"), m_temporal_blending->GetResultTexture()->GetImageView());

            break;
        case EnvGridPassMode::IRRADIANCE:
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("EnvGridIrradianceResultTexture"), GetAttachment(0)->GetImageView());

            break;
        }
    }
}

void EnvGridPass::Resize_Internal(Extent2D new_size)
{
    FullScreenPass::Resize_Internal(new_size);

    if (m_previous_texture.IsValid()) {
        g_safe_deleter->SafeRelease(std::move(m_previous_texture));
    }

    m_render_texture_to_screen_pass.Reset();
    
    m_temporal_blending.Reset();

    CreatePreviousTexture();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();

    AddToGlobalDescriptorSet();
}

void EnvGridPass::Record(uint frame_index)
{
}

void EnvGridPass::Render(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    const uint scene_index = g_engine->GetRenderState().GetScene().id.ToIndex();
    const uint camera_index = g_engine->GetRenderState().GetCamera().id.ToIndex();
    const uint env_grid_index = g_engine->GetRenderState().bound_env_grid.ToIndex();

    GetFramebuffer()->BeginCapture(frame->GetCommandBuffer(), frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame) {
        m_render_texture_to_screen_pass->GetCommandBuffer(frame_index)->Record(
            g_engine->GetGPUInstance()->GetDevice(),
            m_render_group->GetPipeline()->GetRenderPass(),
            [this, frame_index, scene_index, camera_index, env_grid_index](CommandBuffer *cmd)
            {
                // render previous frame's result to screen
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->Bind(cmd);
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                    cmd,
                    frame_index,
                    m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
                    {
                        {
                            NAME("Scene"),
                            {
                                { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                                { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                                { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                                { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, env_grid_index) },
                                { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                            }
                        }
                    }
                );

                m_full_screen_quad->Render(cmd);

                HYPERION_RETURN_OK;
            });

        HYPERION_ASSERT_RESULT(m_render_texture_to_screen_pass->GetCommandBuffer(frame_index)->SubmitSecondary(frame->GetCommandBuffer()));
    } else {
        m_is_first_frame = false;
    }

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];

    command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetRenderPass(),
        [this, frame_index, scene_index, camera_index, env_grid_index](CommandBuffer *cmd)
        {
            const uint global_descriptor_set_index = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
            const uint scene_descriptor_set_index = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

            m_render_group->GetPipeline()->SetPushConstants(
                m_push_constant_data.Data(),
                m_push_constant_data.Size()
            );

            m_render_group->GetPipeline()->Bind(cmd);

            m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->Bind(
                    cmd,
                    m_render_group->GetPipeline(),
                    { },
                    global_descriptor_set_index
                );

            m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                ->Bind(
                    cmd,
                    m_render_group->GetPipeline(),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, env_grid_index) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                    },
                    scene_descriptor_set_index
                );

            m_full_screen_quad->Render(cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(m_command_buffers[frame_index]->SubmitSecondary(frame->GetCommandBuffer()));

    GetFramebuffer()->EndCapture(frame->GetCommandBuffer(), frame_index);

    { // Copy the result to the previous texture
        const ImageRef &src_image = m_framebuffer->GetAttachment(0)->GetImage();
        const ImageRef &dst_image = m_previous_texture->GetImage();

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        dst_image->Blit(
            frame->GetCommandBuffer(),
            src_image
        );

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }

    if (m_temporal_blending) {
        m_temporal_blending->Render(frame);
    }
}

#pragma endregion Env grid pass

#pragma region Reflection probe pass

ReflectionProbePass::ReflectionProbePass()
    : FullScreenPass(InternalFormat::RGBA8_SRGB),
      m_is_first_frame(true)
{
    SetBlendFunction(BlendFunction(
        BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
        BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA
    ));
}

ReflectionProbePass::~ReflectionProbePass()
{
    m_render_texture_to_screen_pass.Reset();

    g_safe_deleter->SafeRelease(std::move(m_previous_texture));

    for (auto &it : m_command_buffers) {
        SafeRelease(std::move(it));
    }
}

void ReflectionProbePass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreatePreviousTexture();
    CreateRenderTextureToScreenPass();

    AddToGlobalDescriptorSet();
}

void ReflectionProbePass::CreatePipeline()
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

void ReflectionProbePass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

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

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

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

void ReflectionProbePass::CreateCommandBuffers()
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    for (uint i = 0; i < ApplyReflectionProbeMode::MAX; i++) {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_command_buffers[i][frame_index] = MakeRenderObject<CommandBuffer>(renderer::CommandBufferType::COMMAND_BUFFER_SECONDARY);

#ifdef HYP_VULKAN
            m_command_buffers[i][frame_index]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[0];
#endif

            DeferCreate(
                m_command_buffers[i][frame_index],
                g_engine->GetGPUDevice()
            );
        }
    }
}

void ReflectionProbePass::CreatePreviousTexture()
{
    // Create previous image
    m_previous_texture = CreateObject<Texture>(Texture2D(
        m_extent,
        m_image_format,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    ));

    InitObject(m_previous_texture);
}

void ReflectionProbePass::CreateRenderTextureToScreenPass()
{
    AssertThrow(m_previous_texture.IsValid());
    AssertThrow(m_previous_texture->GetImageView().IsValid());

    // Create render texture to screen pass.
    // this is used to render the previous frame's result to the screen,
    // so we can blend it with the current frame's result (checkerboarded)
    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), m_previous_texture->GetImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_texture_to_screen_pass.Reset(new FullScreenPass(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
    ));

    m_render_texture_to_screen_pass->Create();
}

void ReflectionProbePass::AddToGlobalDescriptorSet()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("ReflectionProbeResultTexture"), GetAttachment(0)->GetImageView());
    }
}

void ReflectionProbePass::Resize_Internal(Extent2D new_size)
{
    HYP_SCOPE;
    FullScreenPass::Resize_Internal(new_size);

    if (m_previous_texture.IsValid()) {
        g_safe_deleter->SafeRelease(std::move(m_previous_texture));
    }

    m_render_texture_to_screen_pass.Reset();

    CreatePreviousTexture();
    CreateRenderTextureToScreenPass();

    AddToGlobalDescriptorSet();
}

void ReflectionProbePass::Record(uint frame_index)
{
}

void ReflectionProbePass::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    const uint scene_index = g_engine->GetRenderState().GetScene().id.ToIndex();
    const uint camera_index = g_engine->GetRenderState().GetCamera().id.ToIndex();

    // Sky renders first
    static const FixedArray<EnvProbeType, ApplyReflectionProbeMode::MAX> reflection_probe_types {
        ENV_PROBE_TYPE_SKY,
        ENV_PROBE_TYPE_REFLECTION
    };

    static const FixedArray<ApplyReflectionProbeMode, ApplyReflectionProbeMode::MAX> reflection_probe_modes {
        ApplyReflectionProbeMode::DEFAULT,              // ENV_PROBE_TYPE_SKY
        ApplyReflectionProbeMode::PARALLAX_CORRECTED    // ENV_PROBE_TYPE_REFLECTION
    };

    FixedArray<Pair<Handle<RenderGroup> *, Array<ID<EnvProbe>>>, ApplyReflectionProbeMode::MAX> pass_ptrs;

    for (uint mode_index = ApplyReflectionProbeMode::DEFAULT; mode_index < ApplyReflectionProbeMode::MAX; mode_index++) {
        pass_ptrs[mode_index] = {
            &m_render_groups[mode_index],
            { }
        };

        const EnvProbeType env_probe_type = reflection_probe_types[mode_index];

        for (const auto &it : g_engine->render_state.bound_env_probes[env_probe_type]) {
            const ID<EnvProbe> &env_probe_id = it.first;

            pass_ptrs[mode_index].second.PushBack(env_probe_id);
        }
    }
    
    GetFramebuffer()->BeginCapture(frame->GetCommandBuffer(), frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame) {
        m_render_texture_to_screen_pass->GetCommandBuffer(frame_index)->Record(
            g_engine->GetGPUInstance()->GetDevice(),
            m_render_group->GetPipeline()->GetRenderPass(),
            [this, frame_index, scene_index, camera_index](CommandBuffer *cmd)
            {
                // render previous frame's result to screen
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->Bind(cmd);
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                    cmd,
                    frame_index,
                    m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
                    {
                        {
                            NAME("Scene"),
                            {
                                { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                                { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                                { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                                { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                                { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                            }
                        }
                    }
                );

                m_full_screen_quad->Render(cmd);

                HYPERION_RETURN_OK;
            });

        HYPERION_ASSERT_RESULT(m_render_texture_to_screen_pass->GetCommandBuffer(frame_index)->SubmitSecondary(frame->GetCommandBuffer()));
    } else {
        m_is_first_frame = false;
    }
    
    uint num_rendered_env_probes = 0;

    for (uint reflection_probe_type_index = 0; reflection_probe_type_index < ArraySize(reflection_probe_types); reflection_probe_type_index++) {
        const EnvProbeType env_probe_type = reflection_probe_types[reflection_probe_type_index];
        const ApplyReflectionProbeMode mode = reflection_probe_modes[reflection_probe_type_index];

        const Pair<Handle<RenderGroup> *, Array<ID<EnvProbe>>> &it = pass_ptrs[mode];

        if (it.second.Empty()) {
            continue;
        }

        const CommandBufferRef &command_buffer = m_command_buffers[reflection_probe_type_index][frame_index];
        AssertThrow(command_buffer.IsValid());

        const Handle<RenderGroup> &render_group = *it.first;
        const Array<ID<EnvProbe>> &env_probes = it.second;

        const Result record_result = command_buffer->Record(
            g_engine->GetGPUInstance()->GetDevice(),
            render_group->GetPipeline()->GetRenderPass(),
            [this, frame_index, scene_index, camera_index, &render_group, &env_probes, &num_rendered_env_probes](CommandBuffer *cmd)
            {
                render_group->GetPipeline()->SetPushConstants(
                    m_push_constant_data.Data(),
                    m_push_constant_data.Size()
                );
                render_group->GetPipeline()->Bind(cmd);

                const uint global_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
                const uint scene_descriptor_set_index = render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

                render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                    ->Bind(
                        cmd,
                        render_group->GetPipeline(),
                        { },
                        global_descriptor_set_index
                    );

                for (const ID<EnvProbe> env_probe_id : env_probes) {
                    if (num_rendered_env_probes >= max_bound_reflection_probes) {
                        HYP_LOG(Rendering, LogLevel::WARNING, "Attempting to render too many reflection probes.");

                        break;
                    }

                    // TODO: Add visibility check so we skip probes that don't have any impact on the current view

                    render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                        ->Bind(
                            cmd,
                            render_group->GetPipeline(),
                            {
                                { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                                { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, camera_index) },
                                { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                                { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                                { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, env_probe_id.ToIndex()) }
                            },
                            scene_descriptor_set_index
                        );

                    m_full_screen_quad->Render(cmd);

                    ++num_rendered_env_probes;
                }

                HYPERION_RETURN_OK;
            });

        HYPERION_ASSERT_RESULT(record_result);

        HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));
    }

    GetFramebuffer()->EndCapture(frame->GetCommandBuffer(), frame_index);

    { // Copy the result to the previous texture
        const ImageRef &src_image = m_framebuffer->GetAttachment(0)->GetImage();
        const ImageRef &dst_image = m_previous_texture->GetImage();

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

        dst_image->Blit(
            frame->GetCommandBuffer(),
            src_image
        );

        src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
        dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    }
}

#pragma endregion Reflection probe pass

#pragma region Deferred renderer

DeferredRenderer::DeferredRenderer()
    : m_gbuffer(new GBuffer),
      m_is_initialized(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(!m_is_initialized);

    { // initialize GBuffer
        m_gbuffer->Create();

        auto InitializeGBufferFramebuffers = [this]()
        {
            HYP_SCOPE;
            Threads::AssertOnThread(ThreadName::THREAD_RENDER);

            m_opaque_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetFramebuffer();
            m_translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();

            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                uint element_index = 0;

                // not including depth texture here (hence the - 1)
                for (uint attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                    g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                        ->SetElement(NAME("GBufferTextures"), element_index++, m_opaque_fbo->GetAttachment(attachment_index)->GetImageView());
                }

                // add translucent bucket's albedo
                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                    ->SetElement(NAME("GBufferTextures"), element_index++, m_translucent_fbo->GetAttachment(0)->GetImageView());

                // depth attachment goes into separate slot
                const AttachmentRef &depth_attachment = m_opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
                AssertThrow(depth_attachment != nullptr);

                g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                    ->SetElement(NAME("GBufferDepthTexture"), depth_attachment->GetImageView());
            }
        };

        InitializeGBufferFramebuffers();

        m_gbuffer->OnGBufferResolutionChanged.Bind([this, InitializeGBufferFramebuffers](...)
        {
            InitializeGBufferFramebuffers();
        }).Detach();
    }

    m_env_grid_radiance_pass.Reset(new EnvGridPass(EnvGridPassMode::RADIANCE));
    m_env_grid_radiance_pass->Create();

    m_env_grid_irradiance_pass.Reset(new EnvGridPass(EnvGridPassMode::IRRADIANCE));
    m_env_grid_irradiance_pass->Create();

    m_reflection_probe_pass.Reset(new ReflectionProbePass);
    m_reflection_probe_pass->Create();

    m_post_processing.Create();

    m_indirect_pass.Reset(new DeferredPass(DeferredPassMode::INDIRECT_LIGHTING));
    m_indirect_pass->Create();

    m_direct_pass.Reset(new DeferredPass(DeferredPassMode::DIRECT_LIGHTING));
    m_direct_pass->Create();

    const AttachmentRef &depth_attachment = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer()
        ->GetAttachmentMap().attachments.Back().second.attachment;

    AssertThrow(depth_attachment != nullptr);

    m_depth_pyramid_renderer.Reset(new DepthPyramidRenderer);
    m_depth_pyramid_renderer->Create(depth_attachment);

    m_mip_chain = CreateObject<Texture>(Texture2D(
        mip_chain_extent,
        mip_chain_format,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    ));

    InitObject(m_mip_chain);

    m_hbao.Reset(new HBAO());
    m_hbao->Create();

    CreateBlueNoiseBuffer();

    m_ssr.Reset(new SSRRenderer(
        g_engine->GetGPUInstance()->GetSwapchain()->extent,
        SSR_RENDERER_OPTIONS_ROUGHNESS_SCATTERING | SSR_RENDERER_OPTIONS_CONE_TRACING
    ));

    m_ssr->Create();

    // m_dof_blur.Reset(new DOFBlur(g_engine->GetGPUInstance()->GetSwapchain()->extent));
    // m_dof_blur->Create();

    CreateCombinePass();
    CreateDescriptorSets();

    m_temporal_aa.Reset(new TemporalAA(g_engine->GetGPUInstance()->GetSwapchain()->extent));
    m_temporal_aa->Create();

    HYP_SYNC_RENDER();

    m_is_initialized = true;
}

void DeferredRenderer::CreateDescriptorSets()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    // set global gbuffer data
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("GBufferMipChain"), m_mip_chain->GetImageView());

        g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
            ->SetElement(NAME("BlueNoiseBuffer"), m_blue_noise_buffer);
    }
}

void DeferredRenderer::CreateCombinePass()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    ShaderRef shader = g_shader_manager->GetOrCreate(
        NAME("DeferredCombine"),
        GetDeferredShaderProperties()
    );

    AssertThrow(shader.IsValid());

    m_combine_pass.Reset(new FullScreenPass(shader, InternalFormat::RGBA8_SRGB));
    m_combine_pass->Create();

    PUSH_RENDER_COMMAND(
        SetDeferredResultInGlobalDescriptorSet,
        GetCombinedResult()->GetImageView()
    );
}

void DeferredRenderer::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    SafeRelease(std::move(m_blue_noise_buffer));

    m_depth_pyramid_renderer.Reset();

    m_ssr->Destroy();
    m_ssr.Reset();

    m_hbao.Reset();

    m_temporal_aa.Reset();

    // m_dof_blur->Destroy();

    m_post_processing.Destroy();

    m_combine_pass.Reset();

    m_env_grid_radiance_pass.Reset();
    m_env_grid_irradiance_pass.Reset();

    m_reflection_probe_pass.Reset();

    m_mip_chain.Reset();

    m_opaque_fbo.Reset();
    m_translucent_fbo.Reset();

    m_indirect_pass.Reset();
    m_direct_pass.Reset();

    m_gbuffer->Destroy();
}

void DeferredRenderer::Resize(Extent2D new_size)
{
    HYP_SCOPE;
    
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (!m_is_initialized) {
        return;
    }

    m_gbuffer->Resize(new_size);
}

void DeferredRenderer::Render(Frame *frame, RenderEnvironment *environment)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    CommandBuffer *primary = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();
    
    const uint scene_index = g_engine->render_state.GetScene().id.ToIndex();

    const bool do_particles = environment && environment->IsReady();
    const bool do_gaussian_splatting = false;//environment && environment->IsReady();

    const bool use_ssr = !g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool()
        && g_engine->GetAppContext()->GetConfiguration().Get("rendering.ssr.enabled").ToBool();

    const bool use_rt_radiance = g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()
        && (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.path_tracer.enabled").ToBool()
            || g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.reflections.enabled").ToBool());

    const bool use_ddgi = g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()
        && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.gi.enabled").ToBool();
    
    const bool use_hbao = g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbao.enabled").ToBool();
    const bool use_hbil = g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool();

    const bool use_env_grid_irradiance = g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.enabled").ToBool();
    const bool use_env_grid_radiance = g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool();

    const bool use_reflection_probes = g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_SKY].Any()
        || g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any();

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

    deferred_data.flags |= use_ssr && m_ssr->IsRendered() ? DEFERRED_FLAGS_SSR_ENABLED : 0;
    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferred_data.flags |= use_ddgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;

    deferred_data.screen_width = g_engine->GetGPUInstance()->GetSwapchain()->extent.width;
    deferred_data.screen_height = g_engine->GetGPUInstance()->GetSwapchain()->extent.height;

    CollectDrawCalls(frame);

    if (do_particles) {
        environment->GetParticleSystem()->UpdateParticles(frame);
    }

    if (do_gaussian_splatting) {
        environment->GetGaussianSplatting()->UpdateSplats(frame);
    }

    { // indirect lighting
        DebugMarker marker(primary, "Record deferred indirect lighting pass");

        m_indirect_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_indirect_pass->Record(frame_index); // could be moved to only do once
    }

    { // direct lighting
        DebugMarker marker(primary, "Record deferred direct lighting pass");

        m_direct_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_direct_pass->Record(frame_index);
    }

    { // opaque objects
        DebugMarker marker(primary, "Render opaque objects");

        m_opaque_fbo->BeginCapture(primary, frame_index);

        RenderOpaqueObjects(frame);

        m_opaque_fbo->EndCapture(primary, frame_index);
    }
    // end opaque objs

    if (use_env_grid_irradiance) { // submit env grid command buffer
        DebugMarker marker(primary, "Apply env grid irradiance");

        m_env_grid_irradiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_irradiance_pass->Record(frame_index);
        m_env_grid_irradiance_pass->Render(frame);
    }

    if (use_env_grid_radiance) { // submit env grid command buffer
        DebugMarker marker(primary, "Apply env grid radiance");

        m_env_grid_radiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_radiance_pass->Record(frame_index);
        m_env_grid_radiance_pass->Render(frame);
    }

    if (use_reflection_probes) { // submit reflection probes command buffer
        DebugMarker marker(primary, "Apply reflection probes");

        m_reflection_probe_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_reflection_probe_pass->Record(frame_index);
        m_reflection_probe_pass->Render(frame);
    }

    if (use_rt_radiance) {
        DebugMarker marker(primary, "RT Radiance");

        environment->RenderRTRadiance(frame);
    }

    if (use_ddgi) {
        DebugMarker marker(primary, "DDGI");

        environment->RenderDDGIProbes(frame);
    }

    if (use_ssr) { // screen space reflection
        DebugMarker marker(primary, "Screen space reflection");

        Image *mipmapped_result = m_mip_chain->GetImage();

        if (mipmapped_result->GetResourceState() != renderer::ResourceState::UNDEFINED) {
            m_ssr->Render(frame);
        }
    }

    if (use_hbao || use_hbil) {
        m_hbao->Render(frame);
    }

    // Redirect indirect and direct lighting into the same framebuffer
    const FramebufferRef &deferred_pass_framebuffer = m_indirect_pass->GetFramebuffer();

    m_post_processing.RenderPre(frame);

    { // deferred lighting on opaque objects
        DebugMarker marker(primary, "Deferred shading");

        deferred_pass_framebuffer->BeginCapture(primary, frame_index);

        m_indirect_pass->GetCommandBuffer(frame_index)->SubmitSecondary(primary);

        if (g_engine->GetRenderState().lights.Any()) {
            m_direct_pass->GetCommandBuffer(frame_index)->SubmitSecondary(primary);
        }

        deferred_pass_framebuffer->EndCapture(primary, frame_index);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const ImageRef &src_image = deferred_pass_framebuffer->GetAttachment(0)->GetImage();
        GenerateMipChain(frame, src_image);
    }

    { // translucent objects
        DebugMarker marker(primary, "Render translucent objects");

        m_translucent_fbo->BeginCapture(primary, frame_index);

        bool has_set_active_env_probe = false;

        // Set sky environment map as definition
        if (g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
            g_engine->GetRenderState().SetActiveEnvProbe(g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_SKY].Front().first);

            has_set_active_env_probe = true;
        }

        // begin translucent with forward rendering
        RenderTranslucentObjects(frame);

        if (do_particles) {
            environment->GetParticleSystem()->Render(frame);
        }

        if (do_gaussian_splatting) {
            environment->GetGaussianSplatting()->Render(frame);
        }

        if (has_set_active_env_probe) {
            g_engine->GetRenderState().UnsetActiveEnvProbe();
        }

        RenderSkybox(frame);

        // render debug draw
        g_engine->GetDebugDrawer().Render(frame);

        m_translucent_fbo->EndCapture(primary, frame_index);
    }

    {
        struct alignas(128)
        {
            Vec2u   image_dimensions;
            uint32  _pad0, _pad1;
            uint32  deferred_flags;
        } deferred_combine_constants;

        deferred_combine_constants.image_dimensions = {
            m_combine_pass->GetFramebuffer()->GetExtent().width,
            m_combine_pass->GetFramebuffer()->GetExtent().height
        };

        deferred_combine_constants.deferred_flags = deferred_data.flags;

        m_combine_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&deferred_combine_constants, sizeof(deferred_combine_constants));
        m_combine_pass->Begin(frame);

        m_combine_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind(
            m_combine_pass->GetCommandBuffer(frame_index),
            frame_index,
            m_combine_pass->GetRenderGroup()->GetPipeline(),
            {
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                    }
                }
            }
        );

        m_combine_pass->GetQuadMesh()->Render(m_combine_pass->GetCommandBuffer(frame_index));
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

void DeferredRenderer::GenerateMipChain(Frame *frame, Image *src_image)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    CommandBuffer *primary = frame->GetCommandBuffer();
    const uint frame_index = frame->GetFrameIndex();

    const ImageRef &mipmapped_result = m_mip_chain->GetImage();
    AssertThrow(mipmapped_result.IsValid());

    DebugMarker marker(primary, "Mip chain generation");

    // put src image in state for copying from
    src_image->InsertBarrier(primary, renderer::ResourceState::COPY_SRC);
    // put dst image in state for copying to
    mipmapped_result->InsertBarrier(primary, renderer::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    mipmapped_result->Blit(
        primary,
        src_image,
        Rect<uint32> { 0, 0, src_image->GetExtent().width, src_image->GetExtent().height },
        Rect<uint32> { 0, 0, mipmapped_result->GetExtent().width, mipmapped_result->GetExtent().height }
    );

    HYPERION_ASSERT_RESULT(mipmapped_result->GenerateMipmaps(
        g_engine->GetGPUDevice(),
        primary
    ));

    // put src image in state for reading
    src_image->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);
}

void DeferredRenderer::ApplyCameraJitter()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    Vec4f jitter;

    const ID<Camera> camera_id = g_engine->GetRenderState().GetCamera().id;
    const CameraDrawProxy &camera = g_engine->GetRenderState().GetCamera().camera;

    const uint frame_counter = g_engine->GetRenderState().frame_counter + 1;

    static const float jitter_scale = 0.25f;

    if (camera.projection[3][3] < MathUtil::epsilon_f) {
        Matrix4::Jitter(frame_counter, camera.dimensions.width, camera.dimensions.height, jitter);

        g_engine->GetRenderData()->cameras.Get(camera_id.ToIndex()).jitter = jitter * jitter_scale;
        g_engine->GetRenderData()->cameras.MarkDirty(camera_id.ToIndex());
    }
}

void DeferredRenderer::CreateBlueNoiseBuffer()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    
    m_blue_noise_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);

    PUSH_RENDER_COMMAND(CreateBlueNoiseBuffer, m_blue_noise_buffer);
}

void DeferredRenderer::CollectDrawCalls(Frame *frame)
{
    HYP_SCOPE;

    const uint num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (uint index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_SKYBOX) | (1 << BUCKET_TRANSLUCENT)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderSkybox(Frame *frame)
{
    HYP_SCOPE;

    const uint num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (uint index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->ExecuteDrawCalls(
            frame,
            nullptr,
            Bitset((1 << BUCKET_SKYBOX)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderOpaqueObjects(Frame *frame)
{
    HYP_SCOPE;

    const uint num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (uint index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->ExecuteDrawCalls(
            frame,
            nullptr,
            Bitset((1 << BUCKET_OPAQUE)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderTranslucentObjects(Frame *frame)
{
    HYP_SCOPE;

    const uint num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (uint index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->ExecuteDrawCalls(
            frame,
            nullptr,
            Bitset((1 << BUCKET_TRANSLUCENT)),
            &m_cull_data
        );
    }
}

#pragma endregion Deferred pass

} // namespace hyperion
