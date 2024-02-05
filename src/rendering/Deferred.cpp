#include "Deferred.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/BlueNoise.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;
using renderer::Result;
using renderer::GPUBufferType;

const Extent2D DeferredRenderer::mip_chain_extent(512, 512);
const InternalFormat DeferredRenderer::mip_chain_format = InternalFormat::R10G10B10A2;

const Extent2D DeferredRenderer::hbao_extent(512, 512);
const Extent2D DeferredRenderer::ssr_extent(512, 512);

#pragma region Render commands

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

        struct alignas(256) AlignedBuffer
        {
            ShaderVec4<Int32> sobol_256spp_256d[256 * 256 / 4];
            ShaderVec4<Int32> scrambling_tile[128 * 128 * 8 / 4];
            ShaderVec4<Int32> ranking_tile[128 * 128 * 8 / 4];
        };

        static_assert(sizeof(AlignedBuffer::sobol_256spp_256d) == sizeof(BlueNoise::sobol_256spp_256d));
        static_assert(sizeof(AlignedBuffer::scrambling_tile) == sizeof(BlueNoise::scrambling_tile));
        static_assert(sizeof(AlignedBuffer::ranking_tile) == sizeof(BlueNoise::ranking_tile));

        UniquePtr<AlignedBuffer> aligned_buffer(new AlignedBuffer);
        Memory::MemCpy(&aligned_buffer->sobol_256spp_256d[0], BlueNoise::sobol_256spp_256d, sizeof(BlueNoise::sobol_256spp_256d));
        Memory::MemCpy(&aligned_buffer->scrambling_tile[0], BlueNoise::scrambling_tile, sizeof(BlueNoise::scrambling_tile));
        Memory::MemCpy(&aligned_buffer->ranking_tile[0], BlueNoise::ranking_tile, sizeof(BlueNoise::ranking_tile));

        HYPERION_BUBBLE_ERRORS(buffer->Create(g_engine->GetGPUDevice(), sizeof(AlignedBuffer)));

        buffer->Copy(g_engine->GetGPUDevice(), sizeof(AlignedBuffer), aligned_buffer.Get());

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

static ShaderProperties GetDeferredShaderProperties()
{
    ShaderProperties properties;
    properties.Set("RT_REFLECTIONS_ENABLED", g_engine->GetConfig().Get(CONFIG_RT_REFLECTIONS));
    properties.Set("RT_GI_ENABLED", g_engine->GetConfig().Get(CONFIG_RT_GI));
    properties.Set("SSR_ENABLED", g_engine->GetConfig().Get(CONFIG_SSR));
    properties.Set("REFLECTION_PROBE_ENABLED", true);
    properties.Set("ENV_GRID_ENABLED", g_engine->GetConfig().Get(CONFIG_ENV_GRID_GI));
    properties.Set("HBIL_ENABLED", g_engine->GetConfig().Get(CONFIG_HBIL));
    properties.Set("HBAO_ENABLED", g_engine->GetConfig().Get(CONFIG_HBAO));
    properties.Set("LIGHT_RAYS_ENABLED", g_engine->GetConfig().Get(CONFIG_LIGHT_RAYS));
    properties.Set("PATHTRACER", g_engine->GetConfig().Get(CONFIG_PATHTRACER));

    if (g_engine->GetConfig().Get(CONFIG_DEBUG_REFLECTIONS)) {
        properties.Set("DEBUG_REFLECTIONS");
    } else if (g_engine->GetConfig().Get(CONFIG_DEBUG_IRRADIANCE)) {
        properties.Set("DEBUG_IRRADIANCE");
    }

    return properties;
}

DeferredPass::DeferredPass(bool is_indirect_pass)
    : FullScreenPass(InternalFormat::RGBA16F),
      m_is_indirect_pass(is_indirect_pass)
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader()
{
    if (m_is_indirect_pass) {
        m_shader = g_shader_manager->GetOrCreate(
            HYP_NAME(DeferredIndirect),
            GetDeferredShaderProperties()
        );
    } else {
        m_shader = g_shader_manager->GetOrCreate(
            HYP_NAME(DeferredDirect),
            GetDeferredShaderProperties()
        );
    }

    InitObject(m_shader);
}

void DeferredPass::CreateDescriptors()
{
}

void DeferredPass::Create()
{
    CreateShader();
    FullScreenPass::CreateQuad();
    FullScreenPass::CreateCommandBuffers();
    FullScreenPass::CreateFramebuffer();

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .fill_mode = FillMode::FILL,
            .blend_mode = m_is_indirect_pass
                ? BlendMode::NONE
                : BlendMode::ADDITIVE
        }
    );

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void DeferredPass::Record(UInt frame_index)
{
    if (m_is_indirect_pass) {
        FullScreenPass::Record(frame_index);
        
        return;
    }

    // no lights bound, do not render direct shading at all
    if (g_engine->GetRenderState().lights.Empty()) {
        return;
    }

    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            const auto &scene_binding = g_engine->GetRenderState().GetScene();

            const UInt scene_index = scene_binding.id.ToIndex();
            const UInt camera_index = g_engine->GetRenderState().GetCamera().id.ToIndex();

            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif

            // render with each light
            for (const auto &it : g_engine->GetRenderState().lights) {
                const ID<Light> light_id = it.first;
                const LightDrawProxy &light = it.second;

                if (light.visibility_bits & (1ull << SizeType(camera_index))) {
                    // We'll use the EnvProbe slot to bind whatever EnvProbe
                    // is used for the light's shadow map (if applicable)

                    UInt shadow_probe_index = 0;

                    if (light.shadow_map_index != ~0u) {
                        if (light.type == LightType::POINT) {
                            AssertThrow(light.shadow_map_index < max_env_probes);

                            shadow_probe_index = light.shadow_map_index;
                        } else if (light.type == LightType::DIRECTIONAL) {
                            AssertThrow(light.shadow_map_index < max_shadow_maps);
                        }
                    }

                    cmd->BindDescriptorSet(
                        g_engine->GetGPUInstance()->GetDescriptorPool(),
                        m_render_group->GetPipeline(),
                        DescriptorSet::scene_buffer_mapping[frame_index],
                        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                        FixedArray {
                            HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                            HYP_RENDER_OBJECT_OFFSET(Light, light_id.ToIndex()),
                            HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()),
                            HYP_RENDER_OBJECT_OFFSET(EnvProbe, shadow_probe_index),
                            HYP_RENDER_OBJECT_OFFSET(Camera, camera_index)
                        }
                    );

                    m_full_screen_quad->Render(cmd);
                }
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void DeferredPass::Render(Frame *frame)
{
    FullScreenPass::Render(frame);
}

// ===== Env Grid Pass Begin =====

EnvGridPass::EnvGridPass(EnvGridPassMode mode)
    : FullScreenPass(InternalFormat::RGBA16F),
      m_mode(mode)
{
}

EnvGridPass::~EnvGridPass() = default;

void EnvGridPass::CreateShader()
{
    ShaderProperties properties { };

    switch (m_mode) {
    case EnvGridPassMode::ENV_GRID_PASS_MODE_RADIANCE:
        properties.Set("MODE_RADIANCE");

        break;
    case EnvGridPassMode::ENV_GRID_PASS_MODE_IRRADIANCE:
        properties.Set("MODE_IRRADIANCE");

        break;
    }
    
    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(ApplyEnvGrid),
        properties
    );
    
    InitObject(m_shader);
}

void EnvGridPass::Create()
{
    CreateShader();
    FullScreenPass::CreateQuad();
    FullScreenPass::CreateCommandBuffers();
    FullScreenPass::CreateFramebuffer();

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .fill_mode = FillMode::FILL,
            .blend_mode = BlendMode::ADDITIVE
        }
    );

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void EnvGridPass::Record(UInt frame_index)
{
    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            const auto &scene_binding = g_engine->render_state.GetScene();
            const UInt scene_index = scene_binding.id.ToIndex();

            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
            // TODO: Do for each env grid in view
            
            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::scene_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                FixedArray {
                    HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0),
                    HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0),
                    HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                }
            );

            m_full_screen_quad->Render(cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

// ===== Env Grid Pass End =====

// ===== Reflection Probe Pass Begin =====

ReflectionProbePass::ReflectionProbePass()
    : FullScreenPass(InternalFormat::RGBA16F)
{
}

ReflectionProbePass::~ReflectionProbePass() = default;

void ReflectionProbePass::CreateShader()
{
    ShaderProperties properties { };
    
    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(ApplyReflectionProbe),
        properties
    );
    
    InitObject(m_shader);
}

void ReflectionProbePass::Create()
{
    CreateShader();
    FullScreenPass::CreateQuad();
    FullScreenPass::CreateCommandBuffers();
    FullScreenPass::CreateFramebuffer();

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .fill_mode = FillMode::FILL,
            .blend_mode = BlendMode::NORMAL
        }
    );

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void ReflectionProbePass::Record(UInt frame_index)
{
    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            const auto &scene_binding = g_engine->GetRenderState().GetScene();
            const UInt scene_index = scene_binding.id.ToIndex();
            const UInt camera_index = g_engine->GetRenderState().GetCamera().id.ToIndex();

            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
            // Render each reflection probe

            UInt counter = 0;

            for (const auto &it : g_engine->render_state.bound_env_probes[ENV_PROBE_TYPE_REFLECTION]) {
                if (counter >= max_bound_reflection_probes) {
                    DebugLog(
                        LogType::Warn,
                        "Attempting to render too many reflection probes.\n"
                    );

                    break;
                }

                const ID<EnvProbe> &env_probe_id = it.first;

                if (!it.second.HasValue()) {
                    continue;
                }

                // TODO: Add visibility check so we skip probes that don't have any impact on the current view

                cmd->BindDescriptorSet(
                    g_engine->GetGPUInstance()->GetDescriptorPool(),
                    m_render_group->GetPipeline(),
                    DescriptorSet::scene_buffer_mapping[frame_index],
                    DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                    FixedArray {
                        HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                        HYP_RENDER_OBJECT_OFFSET(Light, 0),
                        HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0),
                        HYP_RENDER_OBJECT_OFFSET(EnvProbe, env_probe_id.ToIndex()),
                        HYP_RENDER_OBJECT_OFFSET(Camera, camera_index)
                    }
                );

                m_full_screen_quad->Render(cmd);

                ++counter;
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

// ===== Reflection Probe Pass End =====

DeferredRenderer::DeferredRenderer()
    : m_indirect_pass(true),
      m_direct_pass(false),
      m_env_grid_radiance_pass(EnvGridPassMode::ENV_GRID_PASS_MODE_RADIANCE),
      m_env_grid_irradiance_pass(EnvGridPassMode::ENV_GRID_PASS_MODE_IRRADIANCE)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    m_env_grid_radiance_pass.Create();
    m_env_grid_irradiance_pass.Create();

    m_reflection_probe_pass.Create();

    m_post_processing.Create();
    m_indirect_pass.Create();
    m_direct_pass.Create();

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_opaque_fbo = g_engine->GetDeferredSystem()[Bucket::BUCKET_OPAQUE].GetFramebuffer();
        m_translucent_fbo = g_engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer();
    }
    
    const auto *depth_attachment_usage = g_engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer()->GetAttachmentUsages().Back();
    AssertThrow(depth_attachment_usage != nullptr);

    m_dpr.Create(depth_attachment_usage);

    m_mip_chain = CreateObject<Texture>(Texture2D(
        mip_chain_extent,
        mip_chain_format,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        nullptr
    ));

    InitObject(m_mip_chain);

    m_hbao.Reset(new HBAO(g_engine->GetGPUInstance()->GetSwapchain()->extent / 2));
    m_hbao->Create();
    
    m_indirect_pass.CreateDescriptors(); // no-op
    m_direct_pass.CreateDescriptors();
    
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
}

void DeferredRenderer::CreateDescriptorSets()
{
    // set global gbuffer data
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DescriptorSetRef descriptor_set_globals = g_engine->GetGPUInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);
        
        { // add gbuffer textures
            auto *gbuffer_textures = descriptor_set_globals->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_TEXTURES);

            UInt element_index = 0u;

            // not including depth texture here
            for (UInt attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                gbuffer_textures->SetElementSRV(element_index++, m_opaque_fbo->GetAttachmentUsages()[attachment_index]->GetImageView());
            }

            // add translucent bucket's albedo
            gbuffer_textures->SetElementSRV(element_index++, m_translucent_fbo->GetAttachmentUsages()[0]->GetImageView());
        }

        // depth attachment goes into separate slot
        auto *depth_attachment_usage = m_opaque_fbo->GetAttachmentUsages()[GBUFFER_RESOURCE_MAX - 1];

        /* Depth texture */
        descriptor_set_globals
            ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = depth_attachment_usage->GetImageView()
            });

        /* Mip chain */
        descriptor_set_globals
            ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
            ->SetElementSRV(0, m_mip_chain->GetImageView());

        /* Gbuffer depth sampler */
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_DEPTH_SAMPLER)
            ->SetElementSampler(0, g_engine->GetPlaceholderData()->GetSamplerNearest());

        /* Gbuffer sampler */
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
            ->SetElementSampler(0, g_engine->GetPlaceholderData()->GetSamplerLinearMipmap());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
            ->SetElementSRV(0, m_dpr.GetResultImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_LIGHTING_AMBIENT)
            ->SetElementSRV(0, m_indirect_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_LIGHTING_DIRECT)
            ->SetElementSRV(0, m_direct_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_IRRADIANCE_ACCUM)
            ->SetElementSRV(0, m_env_grid_irradiance_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_RADIANCE)
            ->SetElementSRV(0, m_env_grid_radiance_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_REFLECTION_PROBE)
            ->SetElementSRV(0, m_reflection_probe_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_RESULT)
            ->SetElementSRV(0, m_combine_pass->GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(DescriptorKey::BLUE_NOISE_BUFFER)
            ->SetElementBuffer(0, m_blue_noise_buffer.Get());

        // descriptor_set_globals
        //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DOF_BLUR_HOR)
        //     ->SetElementSRV(0, m_dof_blur->GetHorizontalBlurPass()->GetAttachmentUsage(0)->GetImageView());

        // descriptor_set_globals
        //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DOF_BLUR_VERT)
        //     ->SetElementSRV(0, m_dof_blur->GetVerticalBlurPass()->GetAttachmentUsage(0)->GetImageView());

        // descriptor_set_globals
        //     ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DOF_BLUR_BLENDED)
        //     ->SetElementSRV(0, m_dof_blur->GetCombineBlurPass()->GetAttachmentUsage(0)->GetImageView());
    }
}

void DeferredRenderer::CreateCombinePass()
{
    auto shader = g_shader_manager->GetOrCreate(
        HYP_NAME(DeferredCombine),
        GetDeferredShaderProperties()
    );

    g_engine->InitObject(shader);

    m_combine_pass.Reset(new FullScreenPass(shader, InternalFormat::RGBA16F));
    m_combine_pass->Create();
}

void DeferredRenderer::Destroy()
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    SafeRelease(std::move(m_blue_noise_buffer));

    m_ssr->Destroy();
    m_dpr.Destroy();
    m_hbao->Destroy();
    m_temporal_aa->Destroy();

    // m_dof_blur->Destroy();

    m_post_processing.Destroy();

    m_combine_pass->Destroy();

    m_env_grid_irradiance_pass.Destroy();
    m_env_grid_radiance_pass.Destroy();

    m_reflection_probe_pass.Destroy();

    m_mip_chain.Reset();

    m_opaque_fbo.Reset();
    m_translucent_fbo.Reset();

    m_indirect_pass.Destroy();  // flushes render queue
    m_direct_pass.Destroy();    // flushes render queue
}

void DeferredRenderer::Render(Frame *frame, RenderEnvironment *environment)
{
    Threads::AssertOnThread(THREAD_RENDER);

    CommandBuffer *primary = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    const auto &scene_binding = g_engine->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();

    const bool do_particles = environment && environment->IsReady();
    const bool do_gaussian_splatting = false;//environment && environment->IsReady();

    const bool use_ssr = g_engine->GetConfig().Get(CONFIG_SSR);
    const bool use_rt_radiance = g_engine->GetConfig().Get(CONFIG_RT_REFLECTIONS) || g_engine->GetConfig().Get(CONFIG_PATHTRACER);
    const bool use_ddgi = g_engine->GetConfig().Get(CONFIG_RT_GI);
    const bool use_hbao = g_engine->GetConfig().Get(CONFIG_HBAO);
    const bool use_hbil = g_engine->GetConfig().Get(CONFIG_HBIL);
    const bool use_env_grid_irradiance = g_engine->GetConfig().Get(CONFIG_ENV_GRID_GI);
    const bool use_env_grid_radiance = g_engine->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS);
    const bool use_reflection_probes = g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any();
    const bool use_temporal_aa = g_engine->GetConfig().Get(CONFIG_TEMPORAL_AA) && m_temporal_aa != nullptr;

    if (use_temporal_aa) {
        ApplyCameraJitter();
    }

    struct alignas(128) { UInt32 flags; } deferred_data;

    Memory::MemSet(&deferred_data, 0, sizeof(deferred_data));

    deferred_data.flags |= use_ssr && m_ssr->IsRendered() ? DEFERRED_FLAGS_SSR_ENABLED : 0;
    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferred_data.flags |= use_ddgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;
    
    CollectDrawCalls(frame);
    
    if (do_particles) {
        environment->GetParticleSystem()->UpdateParticles(frame);
    }

    if (do_gaussian_splatting) {
        environment->GetGaussianSplatting()->UpdateSplats(frame);
    }

    { // indirect lighting
        DebugMarker marker(primary, "Record deferred indirect lighting pass");

        m_indirect_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_indirect_pass.Record(frame_index); // could be moved to only do once
    }

    { // direct lighting
        DebugMarker marker(primary, "Record deferred direct lighting pass");
        
        m_direct_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_direct_pass.Record(frame_index);
    }

    { // opaque objects
        DebugMarker marker(primary, "Render opaque objects");

        m_opaque_fbo->BeginCapture(frame_index, primary);
        RenderOpaqueObjects(frame);
        m_opaque_fbo->EndCapture(frame_index, primary);
    }
    // end opaque objs

    if (use_env_grid_irradiance) { // submit env grid command buffer
        DebugMarker marker(primary, "Apply env grid irradiance");

        m_env_grid_irradiance_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_irradiance_pass.Record(frame_index);
        m_env_grid_irradiance_pass.Render(frame);
    }

    if (use_env_grid_radiance) { // submit env grid command buffer
        DebugMarker marker(primary, "Apply env grid radiance");

        m_env_grid_radiance_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_radiance_pass.Record(frame_index);
        m_env_grid_radiance_pass.Render(frame);
    }

    if (use_reflection_probes) { // submit reflection probes command buffer
        DebugMarker marker(primary, "Apply reflection probes");

        m_reflection_probe_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_reflection_probe_pass.Record(frame_index);
        m_reflection_probe_pass.Render(frame);
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

        if (mipmapped_result->GetGPUImage()->GetResourceState() != renderer::ResourceState::UNDEFINED) {
            m_ssr->Render(frame);
        }
    }

    if (use_hbao || use_hbil) {
        m_hbao->Render(frame);
    }
    
    // Redirect indirect and direct lighting into the same framebuffer
    Handle<Framebuffer> &deferred_pass_framebuffer = m_indirect_pass.GetFramebuffer();
    
    m_post_processing.RenderPre(frame);

    { // deferred lighting on opaque objects
        DebugMarker marker(primary, "Deferred shading");

        deferred_pass_framebuffer->BeginCapture(frame_index, primary);

        m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);

        if (g_engine->GetRenderState().lights.Any()) {
            m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);
        }

        deferred_pass_framebuffer->EndCapture(frame_index, primary);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const ImageRef &src_image = deferred_pass_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();
        GenerateMipChain(frame, src_image);
    }

    { // translucent objects
        DebugMarker marker(primary, "Render translucent objects");

        m_translucent_fbo->BeginCapture(frame_index, primary);

        bool has_set_active_env_probe = false;

        if (g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any()) {
            g_engine->GetRenderState().SetActiveEnvProbe(g_engine->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Front().first);

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

        g_engine->GetDebugDrawer().Render(frame);

        m_translucent_fbo->EndCapture(frame_index, primary);
    }

    {
        struct alignas(128) {
            ShaderVec2<UInt32>  image_dimensions;
            UInt32              _pad0, _pad1;
            UInt32              deferred_flags;
        } deferred_combine_constants;

        deferred_combine_constants.image_dimensions = {
            m_combine_pass->GetFramebuffer()->GetExtent().width,
            m_combine_pass->GetFramebuffer()->GetExtent().height
        };

        deferred_combine_constants.deferred_flags = deferred_data.flags;

        m_combine_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&deferred_combine_constants, sizeof(deferred_combine_constants));
        m_combine_pass->Begin(frame);

        m_combine_pass->GetCommandBuffer(frame_index)->BindDescriptorSets(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_combine_pass->GetRenderGroup()->GetPipeline(),
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                HYP_RENDER_OBJECT_OFFSET(Light, 0),
                HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
            }
        );

        m_combine_pass->GetQuadMesh()->Render(m_combine_pass->GetCommandBuffer(frame_index));
        m_combine_pass->End(frame);
    }

    { // render depth pyramid
        m_dpr.Render(frame);
        // update culling info now that depth pyramid has been rendered
        m_cull_data.depth_pyramid_image_view = m_dpr.GetResultImageView();
        m_cull_data.depth_pyramid_dimensions = m_dpr.GetExtent();
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
    CommandBuffer *primary = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    const ImageRef &mipmapped_result = m_mip_chain->GetImage();
    AssertThrow(mipmapped_result.IsValid());

    DebugMarker marker(primary, "Mip chain generation");
    
    // put src image in state for copying from
    src_image->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::COPY_SRC);
    // put dst image in state for copying to
    mipmapped_result->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    mipmapped_result->Blit(
        primary,
        src_image,
        Rect { 0, 0, src_image->GetExtent().width, src_image->GetExtent().height },
        Rect { 0, 0, mipmapped_result->GetExtent().width, mipmapped_result->GetExtent().height }
    );

    HYPERION_ASSERT_RESULT(mipmapped_result->GenerateMipmaps(
        g_engine->GetGPUDevice(),
        primary
    ));

    // put src image in state for reading
    src_image->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);
}

void DeferredRenderer::ApplyCameraJitter()
{
    Vector4 jitter;

    const ID<Camera> camera_id = g_engine->GetRenderState().GetCamera().id;
    const CameraDrawProxy &camera = g_engine->GetRenderState().GetCamera().camera;

    const UInt frame_counter = g_engine->GetRenderState().frame_counter + 1;

    static const Float jitter_scale = 0.25f;

    if (camera.projection[3][3] < MathUtil::epsilon_f) {
        Matrix4::Jitter(frame_counter, camera.dimensions.width, camera.dimensions.height, jitter);

        g_engine->GetRenderData()->cameras.Get(camera_id.ToIndex()).jitter = jitter * jitter_scale;
        g_engine->GetRenderData()->cameras.MarkDirty(camera_id.ToIndex());
    }
}

void DeferredRenderer::CreateBlueNoiseBuffer()
{
    m_blue_noise_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::STORAGE_BUFFER);

    PUSH_RENDER_COMMAND(CreateBlueNoiseBuffer, m_blue_noise_buffer);
}

void DeferredRenderer::CollectDrawCalls(Frame *frame)
{
    const UInt num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (UInt index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->CollectDrawCalls(
            frame,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_SKYBOX) | (1 << BUCKET_TRANSLUCENT)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderOpaqueObjects(Frame *frame)
{
    const UInt num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (UInt index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->ExecuteDrawCalls(
            frame,
            Handle<Framebuffer>::empty,
            Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_SKYBOX)),
            &m_cull_data
        );
    }
}

void DeferredRenderer::RenderTranslucentObjects(Frame *frame)
{
    const UInt num_render_lists = g_engine->GetWorld()->GetRenderListContainer().NumRenderLists();

    for (UInt index = 0; index < num_render_lists; index++) {
        g_engine->GetWorld()->GetRenderListContainer().GetRenderListAtIndex(index)->ExecuteDrawCalls(
            frame,
            Handle<Framebuffer>::empty,
            Bitset((1 << BUCKET_TRANSLUCENT)),
            &m_cull_data
        );
    }
}

} // namespace hyperion::v2
