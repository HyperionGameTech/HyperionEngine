#include "Deferred.hpp"
#include "../Engine.hpp"

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;

DeferredPass::DeferredPass(bool is_indirect_pass)
    : FullScreenPass(Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F),
      m_is_indirect_pass(is_indirect_pass)
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    if (m_is_indirect_pass) {
        m_shader = Handle<Shader>(new Shader(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred.vert.spv")).Read(),
                    {.name = "deferred indirect vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_indirect.frag.spv")).Read(),
                    {.name = "deferred indirect frag"}
                }}
            }
        ));
    } else {
        m_shader = Handle<Shader>(new Shader(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred.vert.spv")).Read(),
                    {.name = "deferred direct vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_direct.frag.spv")).Read(),
                    {.name = "deferred direct frag"}
                }}
            }
        ));
    }

    engine->Attach(m_shader);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = Handle<RenderPass>(engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass());
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    if (m_is_indirect_pass) {
        return;
    }

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();

        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::DEFERRED_RESULT);

            // only add color attachment
            AssertThrowMsg(!framebuffer.GetAttachmentRefs().empty(),
                "Size should be at least 1! Need to have color attachment to create DEFERRED_RESULT descriptor");

            auto *color_attachment_ref = framebuffer.GetAttachmentRefs().front();
            AssertThrow(color_attachment_ref != nullptr);
            AssertThrow(!color_attachment_ref->IsDepthAttachment());

            descriptor->SetSubDescriptor({
                .element_index = 0u,
                .image_view = color_attachment_ref->GetImageView()
            });
        }
    }
}

void DeferredPass::Create(Engine *engine)
{
    CreateQuad(engine);
    CreateShader(engine);
    CreateRenderPass(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[i];
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }

    RenderableAttributeSet renderable_attributes(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes,
            .fill_mode = FillMode::FILL,
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .flags = m_is_indirect_pass
                ? MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
                : MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_ALPHA_BLENDING
        }
    );

    CreatePipeline(engine, renderable_attributes);
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
    if (engine->render_state.light_ids.Empty()) {
        return;
    }

    using renderer::Result;

    auto *command_buffer = m_command_buffers[frame_index].get();

    auto record_result = command_buffer->Record(
        engine->GetInstance()->GetDevice(),
        m_renderer_instance->GetPipeline()->GetConstructionInfo().render_pass,
        [this, engine, frame_index](CommandBuffer *cmd) {
            m_renderer_instance->GetPipeline()->push_constants = m_push_constant_data;
            m_renderer_instance->GetPipeline()->Bind(cmd);

            const auto scene_binding = engine->render_state.GetScene();
            const auto scene_index = scene_binding ? scene_binding.id.value - 1 : 0;

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
            for (const auto &light_id : engine->render_state.light_ids) {
                cmd->BindDescriptorSet(
                    engine->GetInstance()->GetDescriptorPool(),
                    m_renderer_instance->GetPipeline(),
                    DescriptorSet::scene_buffer_mapping[frame_index],
                    DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                    FixedArray {
                        UInt32(sizeof(SceneShaderData) * scene_index),
                        UInt32(sizeof(LightShaderData) * (light_id.value - 1))
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
    
    const auto *depth_attachment_ref = m_indirect_pass.GetRenderPass()->GetRenderPass().GetAttachmentRefs().back();//opaque_render_pass->GetRenderPass().GetAttachmentRefs().back();//.back();
    AssertThrow(depth_attachment_ref != nullptr);

    m_dpr.Create(engine, depth_attachment_ref);
    m_ssr.Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_mipmapped_results[i] = Handle<Texture>(new Texture2D(
            Extent2D { 1024, 1024 },
            Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8_SRGB,
            Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
            Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        engine->Attach(m_mipmapped_results[i]);
    }
    
    m_sampler = std::make_unique<Sampler>(Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP);
    HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetDevice()));

    m_depth_sampler = std::make_unique<Sampler>(Image::FilterMode::TEXTURE_FILTER_NEAREST);
    HYPERION_ASSERT_RESULT(m_depth_sampler->Create(engine->GetDevice()));

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[i];
        
        auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        
        descriptor_set_globals->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_TEXTURES);

        UInt attachment_index = 0;

        /* Gbuffer textures */
        for (; attachment_index < RenderListContainer::gbuffer_textures.size() - 1; attachment_index++) {
            descriptor_set_globals
                ->GetDescriptor(DescriptorKey::GBUFFER_TEXTURES)
                ->SetSubDescriptor({
                    .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView()
                });
        }

        auto *depth_image = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index];

        /* Depth texture */
        descriptor_set_globals
            ->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_DEPTH)
            ->SetSubDescriptor({
                .image_view = depth_image->GetImageView()
            });

        /* Mip chain */
        descriptor_set_globals
            ->AddDescriptor<ImageDescriptor>(DescriptorKey::GBUFFER_MIP_CHAIN)
            ->SetSubDescriptor({
                .image_view = &m_mipmapped_results[i]->GetImageView()
            });

        /* Gbuffer depth sampler */
        descriptor_set_globals
            ->AddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_DEPTH_SAMPLER)
            ->SetSubDescriptor({
                .sampler = m_depth_sampler.get()
            });

        /* Gbuffer sampler */
        descriptor_set_globals
            ->AddDescriptor<renderer::SamplerDescriptor>(DescriptorKey::GBUFFER_SAMPLER)
            ->SetSubDescriptor({
                .sampler = m_sampler.get()
            });

        descriptor_set_globals
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::DEPTH_PYRAMID_RESULT)
            ->SetSubDescriptor({
                .image_view = m_dpr.GetResults()[i].get()
            });
    }
    
    m_indirect_pass.CreateDescriptors(engine); // no-op
    m_direct_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    //! TODO: remove all descriptors

    m_ssr.Destroy(engine);
    m_dpr.Destroy(engine);

    m_post_processing.Destroy(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        engine->SafeReleaseRenderResource<Texture>(std::move(m_mipmapped_results[i]));
    }
    
    HYPERION_ASSERT_RESULT(m_depth_sampler->Destroy(engine->GetDevice()));
    HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetDevice()));

    m_indirect_pass.Destroy(engine);  // flushes render queue
    m_direct_pass.Destroy(engine);    // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    if constexpr (use_draw_indirect) {
        // collect draw calls - not actually rendering yet.
        RenderOpaqueObjects(engine, frame, true);
        RenderTranslucentObjects(engine, frame, true);
    }

    auto &mipmapped_result = m_mipmapped_results[frame_index]->GetImage();

    if (ssr_enabled && mipmapped_result.GetGPUImage()->GetResourceState() != renderer::GPUMemory::ResourceState::UNDEFINED) {
        m_ssr.Render(engine, frame);
    }

    {
        DebugMarker marker(primary, "Record deferred indirect lighting pass");

        m_indirect_pass.m_push_constant_data.deferred_data = {
            .flags = (DeferredRenderer::ssr_enabled && m_ssr.IsRendered())
                ? DEFERRED_FLAGS_SSR_ENABLED
                : 0
        };

        m_indirect_pass.Record(engine, frame_index); // could be moved to only do once
    }

    {
        DebugMarker marker(primary, "Record deferred direct lighting pass");

        m_direct_pass.m_push_constant_data = m_indirect_pass.m_push_constant_data;

        m_direct_pass.Record(engine, frame_index);
    }

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    {
        DebugMarker marker(primary, "Render opaque objects");

        bucket.GetFramebuffers()[frame_index]->BeginCapture(primary);
        RenderOpaqueObjects(engine, frame, false);
        bucket.GetFramebuffers()[frame_index]->EndCapture(primary);
    }
    // end opaque objs
    
    m_post_processing.RenderPre(engine, frame);

    // begin shading
    m_direct_pass.GetFramebuffer(frame_index)->BeginCapture(primary);

    // indirect shading
    HYPERION_ASSERT_RESULT(m_indirect_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));

    // direct shading
    if (!engine->render_state.light_ids.Empty()) {
        HYPERION_ASSERT_RESULT(m_direct_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));
    }

    // begin translucent with forward rendering
    RenderTranslucentObjects(engine, frame, false);

    // end shading
    m_direct_pass.GetFramebuffer(frame_index)->EndCapture(primary);

    // render depth pyramid
    m_dpr.Render(engine, frame);

    // update culling info now that depth pyramid has been rendered
    m_cull_data.depth_pyramid_image_views[frame_index] = m_dpr.GetResults()[frame_index].get();
    m_cull_data.depth_pyramid_dimensions = m_dpr.GetExtent();

    /* ========== BEGIN MIP CHAIN GENERATION ========== */
    {
        DebugMarker marker(primary, "Mipmap chain generation");

        auto *framebuffer_image = m_direct_pass.GetFramebuffer(frame_index)->GetFramebuffer()//bucket.GetFramebuffers()[frame_index]->GetFramebuffer()
            .GetAttachmentRefs()[0]->GetAttachment()->GetImage();
        
        framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_SRC);
        mipmapped_result.GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_DST);

        // Blit into the mipmap chain img
        mipmapped_result.Blit(
            primary,
            framebuffer_image,
            Rect { 0, 0, framebuffer_image->GetExtent().width, framebuffer_image->GetExtent().height },
            Rect { 0, 0, mipmapped_result.GetExtent().width, mipmapped_result.GetExtent().height }
        );

        HYPERION_ASSERT_RESULT(mipmapped_result.GenerateMipmaps(engine->GetDevice(), primary));

        framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    }

    /* ==========  END MIP CHAIN GENERATION ========== */

    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame, bool collect)
{
    if constexpr (use_draw_indirect) {
        if (collect) {
            for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
                pipeline->CollectDrawCalls(engine, frame, m_cull_data);
            }
            
            for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
                pipeline->CollectDrawCalls(engine, frame, m_cull_data);
            }
        } else {
            for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
                pipeline->PerformRendering(engine, frame);
            }
            
            for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
                pipeline->PerformRendering(engine, frame);
            }
        }
    } else {
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetRendererInstances()) {
            pipeline->Render(engine, frame);
        }
        
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
            pipeline->Render(engine, frame);
        }
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame, bool collect)
{
    if constexpr (use_draw_indirect) {
        if (collect) {
            for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
                pipeline->CollectDrawCalls(engine, frame, m_cull_data);
            }
        } else {
            for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
                pipeline->PerformRendering(engine, frame);
            }
        }
    } else {
        for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetRendererInstances()) {
            pipeline->Render(engine, frame);
        }
    }
}

} // namespace hyperion::v2
