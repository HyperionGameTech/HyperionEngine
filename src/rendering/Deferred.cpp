#include "Deferred.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;
using renderer::Result;

const Extent2D DeferredRenderer::mip_chain_extent(512, 512);
const InternalFormat DeferredRenderer::mip_chain_format = InternalFormat::R10G10B10A2;

const Extent2D DeferredRenderer::hbao_extent(1024, 1024);
const Extent2D DeferredRenderer::ssr_extent(512, 512);

static ShaderProps GetDeferredShaderProps()
{
    ShaderProps props;
    props.Set("RT_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_RT_ENABLED));
    props.Set("SSR_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_SSR));
    props.Set("REFLECTION_PROBE_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS));
    props.Set("ENV_GRID_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_ENV_GRID_GI));
    props.Set("VCT_ENABLED_TEXTURE", Engine::Get()->GetConfig().Get(CONFIG_VOXEL_GI));
    props.Set("VCT_ENABLED_SVO", Engine::Get()->GetConfig().Get(CONFIG_VOXEL_GI_SVO));
    props.Set("HBIL_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_HBIL));
    props.Set("HBAO_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_HBAO));


    if (Engine::Get()->GetConfig().Get(CONFIG_DEBUG_REFLECTIONS)) {
        props.Set("DEBUG_REFLECTIONS");
    } else if (Engine::Get()->GetConfig().Get(CONFIG_DEBUG_IRRADIANCE)) {
        props.Set("DEBUG_IRRADIANCE");
    }

    return props;
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
        m_shader = Engine::Get()->GetShaderManager().GetOrCreate(
            HYP_NAME(DeferredIndirect),
            GetDeferredShaderProps()
        );
    } else {
        m_shader = Engine::Get()->GetShaderManager().GetOrCreate(
            HYP_NAME(DeferredDirect),
            GetDeferredShaderProps()
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
    if (Engine::Get()->render_state.lights.Empty()) {
        return;
    }

    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            const auto &scene_binding = Engine::Get()->GetRenderState().GetScene();
            const UInt scene_index = scene_binding.id.ToIndex();
            const UInt camera_index = Engine::Get()->GetRenderState().GetCamera().id.ToIndex();

            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif
            // render with each light
            for (const auto &it : Engine::Get()->GetRenderState().lights) {
                const LightDrawProxy &light = it.second;

                if (light.visibility_bits & (1ull << SizeType(camera_index))) {
                    // We'll use the EnvProbe slot to bind whatever EnvProbe
                    // is used for the light's shadow map (if applicable)

                    UInt shadow_probe_index = 0;

                    if (light.type == LightType::POINT) {
                        shadow_probe_index = light.shadow_map_index;
                    }

                    cmd->BindDescriptorSet(
                        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                        m_render_group->GetPipeline(),
                        DescriptorSet::scene_buffer_mapping[frame_index],
                        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                        FixedArray {
                            HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                            HYP_RENDER_OBJECT_OFFSET(Light, it.first.ToIndex()),
                            HYP_RENDER_OBJECT_OFFSET(EnvGrid, Engine::Get()->GetRenderState().bound_env_grid.ToIndex()),
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

EnvGridPass::EnvGridPass()
    : FullScreenPass(InternalFormat::RGBA16F)
{
}

EnvGridPass::~EnvGridPass() = default;

void EnvGridPass::CreateShader()
{
    ShaderProps props { };
    
    m_shader = Engine::Get()->GetShaderManager().GetOrCreate(
        HYP_NAME(ApplyEnvGrid),
        props
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
        Engine::Get()->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            const auto &scene_binding = Engine::Get()->render_state.GetScene();
            const auto scene_index = scene_binding.id.ToIndex();

            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
            // TODO: Do for each env grid in view
            
            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::scene_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                FixedArray {
                    HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0),
                    HYP_RENDER_OBJECT_OFFSET(EnvGrid, Engine::Get()->GetRenderState().bound_env_grid.ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0),
                    HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
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
    ShaderProps props { };
    
    m_shader = Engine::Get()->GetShaderManager().GetOrCreate(
        HYP_NAME(ApplyReflectionProbe),
        props
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
        Engine::Get()->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            const auto &scene_binding = Engine::Get()->GetRenderState().GetScene();
            const UInt scene_index = scene_binding.id.ToIndex();
            const UInt camera_index = Engine::Get()->GetRenderState().GetCamera().id.ToIndex();

            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
            // Render each reflection probe

            UInt counter = 0;

            for (const auto &it : Engine::Get()->render_state.bound_env_probes[ENV_PROBE_TYPE_REFLECTION]) {
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
                    Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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
      m_direct_pass(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);
    
    m_env_grid_pass.Create();
    m_reflection_probe_pass.Create();

    m_post_processing.Create();
    m_indirect_pass.Create();
    m_direct_pass.Create();

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_opaque_fbo = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_OPAQUE].GetFramebuffer();
        m_translucent_fbo = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer();
    }
    
    const auto *depth_attachment_usage = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffer()->GetAttachmentUsages().back();
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

    m_hbao.Reset(new HBAO(hbao_extent));
    m_hbao->Create();

    m_ssr.Reset(new SSRRenderer(ssr_extent, true));
    m_ssr->Create();
    
    m_indirect_pass.CreateDescriptors(); // no-op
    m_direct_pass.CreateDescriptors();

    CreateCombinePass();
    CreateDescriptorSets();
    
    m_temporal_aa.Reset(new TemporalAA(Engine::Get()->GetGPUInstance()->GetSwapchain()->extent));
    m_temporal_aa->Create();

    HYP_SYNC_RENDER();
}

void DeferredRenderer::CreateDescriptorSets()
{
    // set global gbuffer data
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto *descriptor_set_globals = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
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
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerNearest());

        /* Gbuffer sampler */
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
            ->SetElementSampler(0, &Engine::Get()->GetPlaceholderData().GetSamplerLinearMipmap());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
            ->SetElementSRV(0, m_dpr.GetResults()[frame_index].get());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_LIGHTING_AMBIENT)
            ->SetElementSRV(0, m_indirect_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_LIGHTING_DIRECT)
            ->SetElementSRV(0, m_direct_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_IRRADIANCE_ACCUM)
            ->SetElementSRV(0, m_env_grid_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_REFLECTION_PROBE)
            ->SetElementSRV(0, m_reflection_probe_pass.GetAttachmentUsage(0)->GetImageView());

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_RESULT)
            ->SetElementSRV(0, m_combine_pass->GetAttachmentUsage(0)->GetImageView());
    }
}

void DeferredRenderer::CreateCombinePass()
{
    auto shader = Engine::Get()->GetShaderManager().GetOrCreate(
        HYP_NAME(DeferredCombine),
        GetDeferredShaderProps()
    );

    Engine::Get()->InitObject(shader);

    m_combine_pass.Reset(new FullScreenPass(shader, InternalFormat::RGBA16F));
    m_combine_pass->Create();
}

void DeferredRenderer::Destroy()
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_ssr->Destroy();
    m_dpr.Destroy();
    m_hbao->Destroy();
    m_temporal_aa->Destroy();

    m_post_processing.Destroy();

    m_combine_pass->Destroy();

    m_env_grid_pass.Destroy();
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

    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const UInt scene_index = scene_binding.id.ToIndex();

    const bool do_particles = environment && environment->IsReady();

    const bool use_ssr = Engine::Get()->GetConfig().Get(CONFIG_SSR);
    const bool use_rt_radiance = Engine::Get()->GetConfig().Get(CONFIG_RT_REFLECTIONS);
    const bool use_ddgi = Engine::Get()->GetConfig().Get(CONFIG_RT_GI);
    const bool use_hbao = Engine::Get()->GetConfig().Get(CONFIG_HBAO);
    const bool use_hbil = Engine::Get()->GetConfig().Get(CONFIG_HBIL);
    const bool use_env_grid_irradiance = Engine::Get()->GetConfig().Get(CONFIG_ENV_GRID_GI);
    const bool use_reflection_probes = Engine::Get()->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS) && Engine::Get()->GetRenderState().bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any();
    const bool use_temporal_aa = Engine::Get()->GetConfig().Get(CONFIG_TEMPORAL_AA) && m_temporal_aa != nullptr;

    struct alignas(128) {  UInt32 flags; } deferred_data;

    Memory::Set(&deferred_data, 0, sizeof(deferred_data));

    deferred_data.flags |= use_ssr && m_ssr->IsRendered() ? DEFERRED_FLAGS_SSR_ENABLED : 0;
    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    deferred_data.flags |= use_ddgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;
    
    CollectDrawCalls(frame);

    if (do_particles) {
        UpdateParticles(frame, environment);
    }

    if (use_ssr) { // screen space reflection
        DebugMarker marker(primary, "Screen space reflection");

        Image *mipmapped_result = m_mip_chain->GetImage();

        if (mipmapped_result->GetGPUImage()->GetResourceState() != renderer::ResourceState::UNDEFINED) {
            m_ssr->Render(frame);
        }
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
        DebugMarker marker(primary, "Apply env grid");

        m_env_grid_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_env_grid_pass.Record(frame_index);
        m_env_grid_pass.Render(frame);
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

    if (use_hbao || use_hbil) {
        m_hbao->Render(frame);
    }
    
    m_post_processing.RenderPre(frame);

    auto &deferred_pass_framebuffer = m_indirect_pass.GetFramebuffer();

    { // deferred lighting on opaque objects
        DebugMarker marker(primary, "Deferred shading");

        deferred_pass_framebuffer->BeginCapture(frame_index, primary);

        m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);

        if (Engine::Get()->render_state.lights.Any()) {
            m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);
        }

        deferred_pass_framebuffer->EndCapture(frame_index, primary);
    }

    { // translucent objects
        DebugMarker marker(primary, "Render translucent objects");

        m_translucent_fbo->BeginCapture(frame_index, primary);

        // begin translucent with forward rendering
        RenderTranslucentObjects(frame);

        if (do_particles) {
            RenderParticles(frame, environment);
        }

        Engine::Get()->GetImmediateMode().Render(frame);

        m_translucent_fbo->EndCapture(frame_index, primary);
    }

    {
        struct alignas(128) {
            ShaderVec2<UInt32> image_dimensions;
            UInt32 _pad0, _pad1;
            UInt32 deferred_flags;
        } deferred_combine_constants;

        deferred_combine_constants.image_dimensions = {
            m_combine_pass->GetFramebuffer()->GetExtent().width,
            m_combine_pass->GetFramebuffer()->GetExtent().height
        };

        deferred_combine_constants.deferred_flags = deferred_data.flags;

        m_combine_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&deferred_combine_constants, sizeof(deferred_combine_constants));
        m_combine_pass->Begin(frame);

        m_combine_pass->GetCommandBuffer(frame_index)->BindDescriptorSets(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            m_combine_pass->GetRenderGroup()->GetPipeline(),
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                HYP_RENDER_OBJECT_OFFSET(Light, 0),
                HYP_RENDER_OBJECT_OFFSET(EnvGrid, Engine::Get()->GetRenderState().bound_env_grid.ToIndex()),
                HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0),
                HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
            }
        );

        m_combine_pass->GetQuadMesh()->Render(m_combine_pass->GetCommandBuffer(frame_index));
        m_combine_pass->End(frame);
    }

    { // render depth pyramid
        m_dpr.Render(frame);
        // update culling info now that depth pyramid has been rendered
        m_cull_data.depth_pyramid_image_views[frame_index] = m_dpr.GetResults()[frame_index].get();
        m_cull_data.depth_pyramid_dimensions = m_dpr.GetExtent();
    }

    Image *src_image = deferred_pass_framebuffer->GetAttachmentUsages()[0]->GetAttachment()->GetImage();

    GenerateMipChain(frame, src_image);

    // put src image in state for reading
    src_image->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);

    m_post_processing.RenderPost(frame);

    if (use_temporal_aa) {
        m_temporal_aa->Render(frame);
    }
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
        Engine::Get()->GetGPUDevice(),
        primary
    ));
}

void DeferredRenderer::CollectDrawCalls(Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRenderGroups()) {
            renderer_instance->CollectDrawCalls(frame, m_cull_data);
        }
        
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRenderGroups()) {
            renderer_instance->CollectDrawCalls(frame, m_cull_data);
        }

        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRenderGroups()) {
            renderer_instance->CollectDrawCalls(frame, m_cull_data);
        }
    } else {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRenderGroups()) {
            renderer_instance->CollectDrawCalls(frame);
        }
        
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRenderGroups()) {
            renderer_instance->CollectDrawCalls(frame);
        }

        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRenderGroups()) {
            renderer_instance->CollectDrawCalls(frame);
        }
    }
}

void DeferredRenderer::RenderOpaqueObjects(Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRenderGroups()) {
            render_group->PerformRenderingIndirect(frame);
        }
        
        for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRenderGroups()) {
            render_group->PerformRenderingIndirect(frame);
        }
    } else {
        for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRenderGroups()) {
            render_group->PerformRendering(frame);
        }
        
        for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRenderGroups()) {
            render_group->PerformRendering(frame);
        }
    }
}

void DeferredRenderer::RenderTranslucentObjects(Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRenderGroups()) {
            render_group->PerformRenderingIndirect(frame);
        }
    } else {
        for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRenderGroups()) {
            render_group->PerformRendering(frame);
        }
    }
}

void DeferredRenderer::RenderUI(Frame *frame)
{
    for (auto &render_group : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_UI).GetRenderGroups()) {
        render_group->Render(frame);
    }
}

void DeferredRenderer::UpdateParticles(Frame *frame, RenderEnvironment *environment)
{
    environment->GetParticleSystem()->UpdateParticles(frame);
}

void DeferredRenderer::RenderParticles(Frame *frame, RenderEnvironment *environment)
{
    environment->GetParticleSystem()->Render(frame);
}

} // namespace hyperion::v2
