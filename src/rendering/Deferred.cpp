#include "Deferred.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>


namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;
using renderer::Result;

const Extent2D DeferredRenderer::mipmap_chain_extent(512, 512);
const Extent2D DeferredRenderer::hbao_extent(512, 512);
const Extent2D DeferredRenderer::ssr_extent(1024, 1024);

DeferredPass::DeferredPass(bool is_indirect_pass)
    : FullScreenPass(InternalFormat::RGBA16F),
      m_is_indirect_pass(is_indirect_pass)
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader()
{
    CompiledShader compiled_shader;

    ShaderProps props { };
    props.Set("RT_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_RT_ENABLED));
    props.Set("SSR_ENABLED", Engine::Get()->GetConfig().Get(CONFIG_SSR));
    props.Set("ENV_PROBE_ENABLED", true);

    if (m_is_indirect_pass) {
        compiled_shader = Engine::Get()->GetShaderCompiler().GetCompiledShader(
            "DeferredIndirect",
            props
        );
    } else {
        compiled_shader = Engine::Get()->GetShaderCompiler().GetCompiledShader(
            "DeferredDirect",
            props
        );
    }

    m_shader = Engine::Get()->CreateObject<Shader>(compiled_shader);
    Engine::Get()->InitObject(m_shader);
}

void DeferredPass::CreateRenderPass()
{
    m_render_pass = Handle<RenderPass>(Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass());
}

void DeferredPass::CreateDescriptors()
{
    // if (m_is_indirect_pass) {
    //     return;
    // }

    // for (UInt i = 0; i < max_frames_in_flight; i++) {
    //     auto &framebuffer = m_framebuffers[i]->GetFramebuffer();

    //     if (!framebuffer.GetAttachmentRefs().empty()) {
    //         auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
    //         auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::DEFERRED_RESULT);

    //         // only add color attachment
    //         AssertThrowMsg(!framebuffer.GetAttachmentRefs().empty(),
    //             "Size should be at least 1! Need to have color attachment to create DEFERRED_RESULT descriptor");

    //         auto *color_attachment_ref = framebuffer.GetAttachmentRefs().front();
    //         AssertThrow(color_attachment_ref != nullptr);
    //         AssertThrow(!color_attachment_ref->IsDepthAttachment());

    //         descriptor->SetSubDescriptor({
    //             .element_index = 0u,
    //             .image_view = color_attachment_ref->GetImageView()
    //         });
    //     }
    // }
}

void DeferredPass::Create()
{
    CreateShader();
    FullScreenPass::CreateQuad();
    FullScreenPass::CreateRenderPass();
    FullScreenPass::CreateCommandBuffers();
    FullScreenPass::CreateFramebuffers();

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .fill_mode = FillMode::FILL,
            .flags = m_is_indirect_pass
                ? MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
                : MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING
        }
    );

    FullScreenPass::CreatePipeline(renderable_attributes);
}

void DeferredPass::Destroy()
{
    FullScreenPass::Destroy(); // flushes render queue
}

void DeferredPass::Record(UInt frame_index)
{
    if (m_is_indirect_pass) {
        FullScreenPass::Record(frame_index);
        
        return;
    }

    // no lights bound, do not render direct shading at all
    if (Engine::Get()->render_state.light_bindings.Empty()) {
        return;
    }

    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        m_renderer_instance->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_renderer_instance->GetPipeline()->push_constants = m_push_constant_data;
            m_renderer_instance->GetPipeline()->Bind(cmd);

            const auto &scene_binding = Engine::Get()->render_state.GetScene();
            const auto scene_index = scene_binding.id.ToIndex();

            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif
            // render with each light
            for (const auto &light : Engine::Get()->render_state.light_bindings) {
                cmd->BindDescriptorSet(
                    Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
                    m_renderer_instance->GetPipeline(),
                    DescriptorSet::scene_buffer_mapping[frame_index],
                    DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                    FixedArray {
                        UInt32(sizeof(SceneShaderData) * scene_index),
                        HYP_RENDER_OBJECT_OFFSET(Light, light.id.ToIndex())
                    }
                );

                m_full_screen_quad->Render(cmd);
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void DeferredPass::Render(Frame *frame)
{
    FullScreenPass::Render(frame);
}

DeferredRenderer::DeferredRenderer()
    : m_ssr(ssr_extent),
      m_indirect_pass(true),
      m_direct_pass(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Create();
    m_indirect_pass.Create();
    m_direct_pass.Create();

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_opaque_fbos[frame_index] = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[frame_index];
        AssertThrow(m_opaque_fbos[frame_index]);

        m_translucent_fbos[frame_index] = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[frame_index];
        AssertThrow(m_translucent_fbos[frame_index]);
    }
    
    const auto *depth_attachment_ref = Engine::Get()->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass()->GetRenderPass().GetAttachmentRefs().back();
    AssertThrow(depth_attachment_ref != nullptr);

    m_dpr.Create(depth_attachment_ref);

    m_hbao.Reset(new HBAO(Engine::Get()->GetGPUInstance()->GetSwapchain()->extent / 2));
    m_hbao->Create();

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_results[i] = Engine::Get()->CreateObject<Texture>(
            StorageImage(
                Extent3D(Engine::Get()->GetGPUInstance()->GetSwapchain()->extent),
                InternalFormat::RGBA16F,
                ImageType::TEXTURE_TYPE_2D,
                FilterMode::TEXTURE_FILTER_NEAREST
            ),
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        );

        Engine::Get()->InitObject(m_results[i]);

        m_mipmapped_results[i] = Engine::Get()->CreateObject<Texture>(Texture2D(
            mipmap_chain_extent,
            InternalFormat::RGBA8_SRGB,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        Engine::Get()->InitObject(m_mipmapped_results[i]);
    }

    m_ssr.Create();
    
    m_sampler = UniquePtr<Sampler>::Construct(FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP);
    HYPERION_ASSERT_RESULT(m_sampler->Create(Engine::Get()->GetGPUDevice()));

    m_depth_sampler = UniquePtr<Sampler>::Construct(FilterMode::TEXTURE_FILTER_NEAREST);
    HYPERION_ASSERT_RESULT(m_depth_sampler->Create(Engine::Get()->GetGPUDevice()));
    
    m_indirect_pass.CreateDescriptors(); // no-op
    m_direct_pass.CreateDescriptors();
    
    m_temporal_aa.Reset(new TemporalAA(Engine::Get()->GetGPUInstance()->GetSwapchain()->extent));
    m_temporal_aa->Create();

    HYP_FLUSH_RENDER_QUEUE();

    CreateDescriptorSets();
    CreateComputePipelines();

    //if (Engine::Get()->GetConfig().Get(CONFIG_RT_SUPPORTED)) {
    //    m_rt_radiance.Create;
    //}
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
                gbuffer_textures->SetSubDescriptor({
                    .element_index = element_index,
                    .image_view = m_opaque_fbos[frame_index]->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
                });

                ++element_index;
            }

            // add translucent bucket's albedo
            gbuffer_textures->SetSubDescriptor({
                .element_index = element_index,
                .image_view = m_translucent_fbos[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView()
            });

            ++element_index;
        }

        // depth attachment goes into separate slot
        auto *depth_attachment_ref = m_opaque_fbos[frame_index]->GetFramebuffer().GetAttachmentRefs()[GBUFFER_RESOURCE_MAX - 1];

        /* Depth texture */
        descriptor_set_globals
            ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = depth_attachment_ref->GetImageView()
            });

        /* Mip chain */
        descriptor_set_globals
            ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_mipmapped_results[frame_index]->GetImageView()
            });

        /* Gbuffer depth sampler */
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_DEPTH_SAMPLER)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = m_depth_sampler.Get()
            });

        /* Gbuffer sampler */
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = m_sampler.Get()
            });

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_dpr.GetResults()[frame_index].get()
            });
            
        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEFERRED_RESULT)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_results[frame_index]->GetImageView()
            });
    }
    
    // create descriptor sets for combine pass (compute shader)
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        // indirect lighting
        descriptor_set
            ->AddDescriptor<renderer::ImageDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_indirect_pass.GetFramebuffer(frame_index)->GetFramebuffer()
                    .GetAttachmentRefs()[0]->GetImageView()
            });

        // direct lighting
        descriptor_set
            ->AddDescriptor<renderer::ImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_direct_pass.GetFramebuffer(frame_index)->GetFramebuffer()
                    .GetAttachmentRefs()[0]->GetImageView()
            });

        // mip chain
        descriptor_set
            ->AddDescriptor<renderer::ImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_mipmapped_results[frame_index]->GetImageView()
            });

        // nearest sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerNearest()
            });

        // linear sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerLinear()
            });

        // output result
        descriptor_set
            ->AddDescriptor<renderer::StorageImageDescriptor>(5)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_results[frame_index]->GetImageView()
            });

        // scene data (for camera matrices)
        descriptor_set
            ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(6)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = Engine::Get()->GetRenderData()->scenes.GetBuffers()[frame_index].get(),
                .range = static_cast<UInt32>(sizeof(SceneShaderData))
            });

        { // gbuffer textures
            auto *gbuffer_textures = descriptor_set->AddDescriptor<renderer::ImageDescriptor>(7);

            UInt element_index = 0u;

            // not including depth texture here
            for (UInt attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                gbuffer_textures->SetSubDescriptor({
                    .element_index = element_index,
                    .image_view = m_opaque_fbos[frame_index]->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
                });

                ++element_index;
            }

            // add translucent bucket's albedo
            gbuffer_textures->SetSubDescriptor({
                .element_index = element_index,
                .image_view = m_translucent_fbos[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView()
            });

            ++element_index;
        }

        // add depth texture
        auto *depth_attachment_ref = m_opaque_fbos[frame_index]->GetFramebuffer().GetAttachmentRefs()[GBUFFER_RESOURCE_MAX - 1];

        /* Depth texture */
        descriptor_set
            ->AddDescriptor<ImageDescriptor>(8)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = depth_attachment_ref->GetImageView()
            });

        HYPERION_ASSERT_RESULT(descriptor_set->Create(
            Engine::Get()->GetGPUDevice(),
            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
        ));

        m_combine_descriptor_sets[frame_index] = std::move(descriptor_set);
    }
}

void DeferredRenderer::CreateComputePipelines()
{
    m_combine = Engine::Get()->CreateObject<ComputePipeline>(
        Engine::Get()->CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("DeferredCombine")),
        Array<const DescriptorSet *> { m_combine_descriptor_sets[0].Get() }
    );

    Engine::Get()->InitObject(m_combine);
}

void DeferredRenderer::Destroy()
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_ssr.Destroy();
    m_dpr.Destroy();
    m_hbao->Destroy();
    m_temporal_aa->Destroy();

    m_post_processing.Destroy();

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        Engine::Get()->SafeRelease(std::move(m_combine_descriptor_sets[frame_index]));

        Engine::Get()->SafeReleaseHandle<Texture>(std::move(m_results[frame_index]));
        Engine::Get()->SafeReleaseHandle<Texture>(std::move(m_mipmapped_results[frame_index]));

        AssertThrow(m_results[frame_index] == nullptr);

        m_opaque_fbos[frame_index].Reset();
        m_translucent_fbos[frame_index].Reset();
    }

    Engine::Get()->SafeRelease(std::move(m_sampler));
    Engine::Get()->SafeRelease(std::move(m_depth_sampler));

    m_combine.Reset();

    m_indirect_pass.Destroy();  // flushes render queue
    m_direct_pass.Destroy();    // flushes render queue
}

void DeferredRenderer::Render(
    
    Frame *frame,
    RenderEnvironment *environment
)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    const auto &scene_binding = Engine::Get()->render_state.GetScene();
    const auto scene_index = scene_binding.id.ToIndex();

    const bool do_particles = environment && environment->IsReady();

    const bool use_ssr = Engine::Get()->GetConfig().Get(CONFIG_SSR);
    const bool use_rt_radiance = Engine::Get()->GetConfig().Get(CONFIG_RT_REFLECTIONS);
    const bool use_hbao = Engine::Get()->GetConfig().Get(CONFIG_HBAO);
    const bool use_hbil = Engine::Get()->GetConfig().Get(CONFIG_HBIL);

    struct alignas(128) { UInt32 flags; } deferred_data;
    deferred_data.flags = 0;
    deferred_data.flags |= use_ssr && m_ssr.IsRendered() ? DEFERRED_FLAGS_SSR_ENABLED : 0;
    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
    
    CollectDrawCalls(frame);

    if (do_particles) {
        UpdateParticles(frame, environment);
    }

    if (use_ssr) { // screen space reflection
        DebugMarker marker(primary, "Screen space reflection");

        auto &mipmapped_result = m_mipmapped_results[frame_index]->GetImage();

        if (mipmapped_result.GetGPUImage()->GetResourceState() != renderer::ResourceState::UNDEFINED) {
            m_ssr.Render(frame);
        }
    } else if (use_rt_radiance) { // rt radiance
        DebugMarker marker(primary, "RT Radiance");

        environment->RenderRTRadiance(frame);
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

        m_opaque_fbos[frame_index]->BeginCapture(primary);
        RenderOpaqueObjects(frame);
        m_opaque_fbos[frame_index]->EndCapture(primary);
    }
    // end opaque objs

    if (use_hbao || use_hbil) {
        m_hbao->Render(frame);
    }
    
    m_post_processing.RenderPre(frame);

    auto &deferred_pass_framebuffer = m_indirect_pass.GetFramebuffer(frame_index);

    { // deferred lighting on opaque objects
        DebugMarker marker(primary, "Deferred shading");

        deferred_pass_framebuffer->BeginCapture(primary);

        m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);

        if (Engine::Get()->render_state.light_bindings.Any()) {
            m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);
        }

        deferred_pass_framebuffer->EndCapture(primary);
    }

    { // translucent objects
        DebugMarker marker(primary, "Render translucent objects");

        m_translucent_fbos[frame_index]->BeginCapture(primary);

        // begin translucent with forward rendering
        RenderTranslucentObjects(frame);

        if (do_particles) {
            RenderParticles(frame, environment);
        }

        Engine::Get()->GetImmediateMode().Render(frame);

        m_translucent_fbos[frame_index]->EndCapture(primary);
    }

    // combine opaque with translucent
    m_results[frame_index]->GetImage().GetGPUImage()->InsertBarrier(
        primary,
        renderer::ResourceState::UNORDERED_ACCESS
    );

    m_combine->GetPipeline()->Bind(
        primary,
        renderer::Pipeline::PushConstantData {
            .deferred_combine_data = {
                .image_dimensions = {
                    m_results[frame_index]->GetExtent().width,
                    m_results[frame_index]->GetExtent().height
                }
            }
        }
    );

    primary->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_combine->GetPipeline(),
        m_combine_descriptor_sets[frame_index].Get(),
        static_cast<DescriptorSet::Index>(0),
        FixedArray { static_cast<UInt32>(scene_index * sizeof(SceneShaderData)) }
    );

    // TODO: benchmark difference vs using a framebuffer and just drawing another quad

    m_combine->GetPipeline()->Dispatch(
        primary,
        Extent3D {
            ((m_results[frame_index]->GetExtent().width) + 31) / 32,
            ((m_results[frame_index]->GetExtent().height) + 31) / 32,
            1
        }
    );

    { // render depth pyramid
        m_dpr.Render(frame);
        // update culling info now that depth pyramid has been rendered
        m_cull_data.depth_pyramid_image_views[frame_index] = m_dpr.GetResults()[frame_index].get();
        m_cull_data.depth_pyramid_dimensions = m_dpr.GetExtent();
    }

    auto *src_image = deferred_pass_framebuffer->GetRenderPass()
        ->GetRenderPass().GetAttachmentRefs()[0]->GetAttachment()->GetImage();

    GenerateMipChain(frame, src_image);

    // put src image in state for reading
    src_image->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);
    m_results[frame_index]->GetImage().GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);

    m_post_processing.RenderPost(frame);
    
    m_temporal_aa->Render(frame);
}

void DeferredRenderer::GenerateMipChain(Frame *frame, Image *src_image)
{
    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    auto &mipmapped_result = m_mipmapped_results[frame_index]->GetImage();

    DebugMarker marker(primary, "Mip chain generation");
    
    // put src image in state for copying from
    src_image->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::COPY_SRC);
    // put dst image in state for copying to
    mipmapped_result.GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    mipmapped_result.Blit(
        primary,
        src_image,
        Rect { 0, 0, src_image->GetExtent().width, src_image->GetExtent().height },
        Rect { 0, 0, mipmapped_result.GetExtent().width, mipmapped_result.GetExtent().height }
    );

    HYPERION_ASSERT_RESULT(mipmapped_result.GenerateMipmaps(
        Engine::Get()->GetGPUDevice(),
        primary
    ));
}

void DeferredRenderer::CollectDrawCalls(Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(frame, m_cull_data);
        }
        
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(frame, m_cull_data);
        }

        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(frame, m_cull_data);
        }
    } else {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(frame);
        }
        
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(frame);
        }

        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(frame);
        }
    }
}

void DeferredRenderer::RenderOpaqueObjects(Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->PerformRenderingIndirect(frame);
        }
        
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->PerformRenderingIndirect(frame);
        }
    } else {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->PerformRendering(frame);
        }
        
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->PerformRendering(frame);
        }
    }
}

void DeferredRenderer::RenderTranslucentObjects(Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->PerformRenderingIndirect(frame);
        }
    } else {
        for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->PerformRendering(frame);
        }
    }
}

void DeferredRenderer::RenderUI(Frame *frame)
{
    for (auto &renderer_instance : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_UI).GetRendererInstances()) {
        renderer_instance->Render(frame);
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
