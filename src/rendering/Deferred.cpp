#include "Deferred.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/backend/vulkan/RendererFeatures.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>


namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;
using renderer::Result;

const Extent2D DeferredRenderer::mipmap_chain_extent(512, 512);

DeferredPass::DeferredPass(bool is_indirect_pass)
    : FullScreenPass(InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F),
      m_is_indirect_pass(is_indirect_pass)
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    CompiledShader compiled_shader;

    ShaderProps props { };
    props.Set("RT_ENABLED", engine->GetConfig().Get(CONFIG_RT_ENABLED));
    props.Set("SSR_ENABLED", engine->GetConfig().Get(CONFIG_SSR));

    if (m_is_indirect_pass) {
        compiled_shader = engine->GetShaderCompiler().GetCompiledShader(
            "DeferredIndirect",
            props
        );
    } else {
        compiled_shader = engine->GetShaderCompiler().GetCompiledShader(
            "DeferredDirect",
            props
        );
    }

    m_shader = engine->CreateHandle<Shader>(compiled_shader);
    engine->InitObject(m_shader);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = Handle<RenderPass>(engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass());
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    // if (m_is_indirect_pass) {
    //     return;
    // }

    // for (UInt i = 0; i < max_frames_in_flight; i++) {
    //     auto &framebuffer = m_framebuffers[i]->GetFramebuffer();

    //     if (!framebuffer.GetAttachmentRefs().empty()) {
    //         auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
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

void DeferredPass::Create(Engine *engine)
{
    CreateShader(engine);
    FullScreenPass::CreateQuad(engine);
    FullScreenPass::CreateRenderPass(engine);
    FullScreenPass::CreateCommandBuffers(engine);
    FullScreenPass::CreateFramebuffers(engine);

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

    FullScreenPass::CreatePipeline(engine, renderable_attributes);
}

void DeferredPass::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Record(Engine *engine, UInt frame_index)
{
    if (m_is_indirect_pass) {
        FullScreenPass::Record(engine, frame_index);
        
        return;
    }

    // no lights bound, do not render direct shading at all
    if (engine->render_state.light_bindings.Empty()) {
        return;
    }

    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        engine->GetInstance()->GetDevice(),
        m_renderer_instance->GetPipeline()->GetConstructionInfo().render_pass,
        [this, engine, frame_index](CommandBuffer *cmd) {
            m_renderer_instance->GetPipeline()->push_constants = m_push_constant_data;
            m_renderer_instance->GetPipeline()->Bind(cmd);

            const auto &scene_binding = engine->render_state.GetScene();
            const auto scene_index = scene_binding.id.ToIndex();

            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif
            // render with each light
            for (const auto &light : engine->render_state.light_bindings) {
                cmd->BindDescriptorSet(
                    engine->GetInstance()->GetDescriptorPool(),
                    m_renderer_instance->GetPipeline(),
                    DescriptorSet::scene_buffer_mapping[frame_index],
                    DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                    FixedArray {
                        UInt32(sizeof(SceneShaderData) * scene_index),
                        HYP_RENDER_OBJECT_OFFSET(Light, light.id.ToIndex())
                    }
                );

                m_full_screen_quad->Render(engine, cmd);
            }

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
    FullScreenPass::Render(engine, frame);
}

DeferredRenderer::DeferredRenderer()
    : m_ssr(Extent2D { 512, 512 }),
      m_indirect_pass(true),
      m_direct_pass(false)
{
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Create(engine);

    m_indirect_pass.Create(engine);
    m_direct_pass.Create(engine);

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_opaque_fbos[frame_index] = engine->GetDeferredSystem()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[frame_index];
        AssertThrow(m_opaque_fbos[frame_index]);

        m_translucent_fbos[frame_index] = engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[frame_index];
        AssertThrow(m_translucent_fbos[frame_index]);
    }
    
    const auto *depth_attachment_ref = engine->GetDeferredSystem()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass()->GetRenderPass().GetAttachmentRefs().back();
    AssertThrow(depth_attachment_ref != nullptr);

    m_dpr.Create(engine, depth_attachment_ref);
    m_ssr.Create(engine);

    m_hbao.Reset(new HBAO(engine->GetInstance()->GetSwapchain()->extent));
    m_hbao->Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_results[i] = engine->CreateHandle<Texture>(
            StorageImage(
                Extent3D(engine->GetInstance()->GetSwapchain()->extent),
                InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                ImageType::TEXTURE_TYPE_2D,
                FilterMode::TEXTURE_FILTER_NEAREST
            ),
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        );

        engine->InitObject(m_results[i]);

        m_mipmapped_results[i] = engine->CreateHandle<Texture>(new Texture2D(
            mipmap_chain_extent,
            InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
            FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        engine->InitObject(m_mipmapped_results[i]);
    }
    
    m_sampler = UniquePtr<Sampler>::Construct(FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP);
    HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetDevice()));

    m_depth_sampler = UniquePtr<Sampler>::Construct(FilterMode::TEXTURE_FILTER_NEAREST);
    HYPERION_ASSERT_RESULT(m_depth_sampler->Create(engine->GetDevice()));
    
    m_indirect_pass.CreateDescriptors(engine); // no-op
    m_direct_pass.CreateDescriptors(engine);
    
    m_temporal_aa.Reset(new TemporalAA(engine->GetInstance()->GetSwapchain()->extent));
    m_temporal_aa->Create(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);

    CreateDescriptorSets(engine);
    CreateComputePipelines(engine);

    //if (engine->GetConfig().Get(CONFIG_RT_SUPPORTED)) {
    //    m_rt_radiance.Create(engine);
    //}
}

void DeferredRenderer::CreateDescriptorSets(Engine *engine)
{
    // set global gbuffer data
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
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
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
            });

        // linear sampler
        descriptor_set
            ->AddDescriptor<renderer::SamplerDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &engine->GetPlaceholderData().GetSamplerLinear()
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
                .buffer = engine->GetRenderData()->scenes.GetBuffers()[frame_index].get(),
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
            engine->GetDevice(),
            &engine->GetInstance()->GetDescriptorPool()
        ));

        m_combine_descriptor_sets[frame_index] = std::move(descriptor_set);
    }
}

void DeferredRenderer::CreateComputePipelines(Engine *engine)
{
    m_combine = engine->CreateHandle<ComputePipeline>(
        engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("DeferredCombine")),
        Array<const DescriptorSet *> { m_combine_descriptor_sets[0].Get() }
    );

    engine->InitObject(m_combine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_ssr.Destroy(engine);
    m_dpr.Destroy(engine);
    m_hbao->Destroy(engine);
    m_temporal_aa->Destroy(engine);

    m_post_processing.Destroy(engine);

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        engine->SafeRelease(std::move(m_combine_descriptor_sets[frame_index]));

        engine->SafeReleaseHandle<Texture>(std::move(m_results[frame_index]));
        engine->SafeReleaseHandle<Texture>(std::move(m_mipmapped_results[frame_index]));

        m_opaque_fbos[frame_index].Reset();
        m_translucent_fbos[frame_index].Reset();
    }

    engine->SafeRelease(std::move(m_sampler));
    engine->SafeRelease(std::move(m_depth_sampler));

    m_combine.Reset();

    m_indirect_pass.Destroy(engine);  // flushes render queue
    m_direct_pass.Destroy(engine);    // flushes render queue
}

void DeferredRenderer::Render(
    Engine *engine,
    Frame *frame,
    RenderEnvironment *environment
)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    const auto &scene_binding = engine->render_state.GetScene();
    const auto scene_index = scene_binding.id.ToIndex();

    const bool do_particles = environment && environment->IsReady();

    const bool use_ssr = engine->GetConfig().Get(CONFIG_SSR);
    const bool use_rt_radiance = engine->GetConfig().Get(CONFIG_RT_REFLECTIONS);
    const bool use_hbao = engine->GetConfig().Get(CONFIG_HBAO);
    const bool use_hbil = engine->GetConfig().Get(CONFIG_HBIL);

    struct alignas(128) { UInt32 flags; } deferred_data;
    deferred_data.flags = 0;
    deferred_data.flags |= use_ssr && m_ssr.IsRendered() ? DEFERRED_FLAGS_SSR_ENABLED : 0;
    deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
    deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
    
    CollectDrawCalls(engine, frame);

    if (do_particles) {
        UpdateParticles(engine, frame, environment);
    }

    if (use_ssr) { // screen space reflection
        DebugMarker marker(primary, "Screen space reflection");

        auto &mipmapped_result = m_mipmapped_results[frame_index]->GetImage();

        if (mipmapped_result.GetGPUImage()->GetResourceState() != renderer::ResourceState::UNDEFINED) {
            m_ssr.Render(engine, frame);
        }
    } else if (use_rt_radiance) { // rt radiance
        DebugMarker marker(primary, "RT Radiance");

        environment->RenderRTRadiance(engine, frame);
    }

    { // indirect lighting
        DebugMarker marker(primary, "Record deferred indirect lighting pass");

        m_indirect_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_indirect_pass.Record(engine, frame_index); // could be moved to only do once
    }

    { // direct lighting
        DebugMarker marker(primary, "Record deferred direct lighting pass");
        
        m_direct_pass.SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_direct_pass.Record(engine, frame_index);
    }

    { // opaque objects
        DebugMarker marker(primary, "Render opaque objects");

        m_opaque_fbos[frame_index]->BeginCapture(primary);
        RenderOpaqueObjects(engine, frame);
        m_opaque_fbos[frame_index]->EndCapture(primary);
    }
    // end opaque objs

    if (use_hbao || use_hbil) {
        m_hbao->Render(engine, frame);
    }
    
    m_post_processing.RenderPre(engine, frame);

    auto &deferred_pass_framebuffer = m_indirect_pass.GetFramebuffer(frame_index);

    { // deferred lighting on opaque objects
        DebugMarker marker(primary, "Deferred shading");

        deferred_pass_framebuffer->BeginCapture(primary);

        m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);

        if (engine->render_state.light_bindings.Any()) {
            m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary);
        }

        deferred_pass_framebuffer->EndCapture(primary);
    }

    { // translucent objects
        DebugMarker marker(primary, "Render translucent objects");

        m_translucent_fbos[frame_index]->BeginCapture(primary);

        // begin translucent with forward rendering
        RenderTranslucentObjects(engine, frame);

        if (do_particles) {
            RenderParticles(engine, frame, environment);
        }

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
        engine->GetInstance()->GetDescriptorPool(),
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
        m_dpr.Render(engine, frame);
        // update culling info now that depth pyramid has been rendered
        m_cull_data.depth_pyramid_image_views[frame_index] = m_dpr.GetResults()[frame_index].get();
        m_cull_data.depth_pyramid_dimensions = m_dpr.GetExtent();
    }

    auto *src_image = deferred_pass_framebuffer->GetRenderPass()
        ->GetRenderPass().GetAttachmentRefs()[0]->GetAttachment()->GetImage();

    GenerateMipChain(engine, frame, src_image);

    // put src image in state for reading
    src_image->GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);
    m_results[frame_index]->GetImage().GetGPUImage()->InsertBarrier(primary, renderer::ResourceState::SHADER_RESOURCE);

    m_post_processing.RenderPost(engine, frame);
    
    m_temporal_aa->Render(engine, frame);
}

void DeferredRenderer::GenerateMipChain(Engine *engine, Frame *frame, Image *src_image)
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
        engine->GetDevice(),
        primary
    ));
}

void DeferredRenderer::CollectDrawCalls(Engine *engine, Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(engine, frame, m_cull_data);
        }
        
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(engine, frame, m_cull_data);
        }

        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(engine, frame, m_cull_data);
        }
    } else {
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(engine, frame);
        }
        
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(engine, frame);
        }

        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->CollectDrawCalls(engine, frame);
        }
    }
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->PerformRenderingIndirect(engine, frame);
        }
        
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->PerformRenderingIndirect(engine, frame);
        }
    } else {
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            renderer_instance->PerformRendering(engine, frame);
        }
        
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            renderer_instance->PerformRendering(engine, frame);
        }
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    if constexpr (use_draw_indirect) {
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->PerformRenderingIndirect(engine, frame);
        }
    } else {
        for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            renderer_instance->PerformRendering(engine, frame);
        }
    }
}

void DeferredRenderer::RenderUI(Engine *engine, Frame *frame)
{
    for (auto &renderer_instance : engine->GetDeferredSystem().Get(Bucket::BUCKET_UI).GetRendererInstances()) {
        renderer_instance->Render(engine, frame);
    }
}

void DeferredRenderer::UpdateParticles(Engine *engine, Frame *frame, RenderEnvironment *environment)
{
    environment->GetParticleSystem()->UpdateParticles(engine, frame);
}

void DeferredRenderer::RenderParticles(Engine *engine, Frame *frame, RenderEnvironment *environment)
{
    environment->GetParticleSystem()->Render(engine, frame);
}

} // namespace hyperion::v2
