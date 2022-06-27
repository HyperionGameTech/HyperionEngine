#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

namespace hyperion::v2 {

using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;

DeferredPass::DeferredPass()
    : FullScreenPass()
{
    
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_vert.spv")).Read(),
                {.name = "deferred vert"}
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_frag.spv")).Read(),
                {.name = "deferred frag"}
            }}
        }
    ));

    m_shader->Init(engine);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass().IncRef();
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->SetSubDescriptor({
                    .element_index = ~0u,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler(),
                });
            }
        }
    }
}

void DeferredPass::Create(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[i].IncRef();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }
    
    CreatePipeline(engine);
}

void DeferredPass::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer() = default;
DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Create(engine);

    m_pass.CreateShader(engine);
    m_pass.CreateRenderPass(engine);
    m_pass.Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[i];

        /* Add our gbuffer textures */
        auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

        /* Albedo texture */
        descriptor_set_pass
            ->AddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView()
            });

        /* Normals texture*/
        descriptor_set_pass
            ->GetDescriptor(0)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[1]->GetImageView()
            });

        /* Position texture */
        descriptor_set_pass
            ->GetDescriptor(0)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[2]->GetImageView()
            });

        /* Material ID */
        descriptor_set_pass
            ->GetDescriptor(0)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[3]->GetImageView()
            });

        /* Depth texture */
        descriptor_set_pass
            ->AddDescriptor<ImageDescriptor>(1)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[4]->GetImageView()
            });
    }
    
    m_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Destroy(engine);
    m_pass.Destroy(engine);  // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    m_pass.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    bucket.GetFramebuffers()[frame_index]->BeginCapture(primary); /* TODO: frame index? */
    RenderOpaqueObjects(engine, frame);
    bucket.GetFramebuffers()[frame_index]->EndCapture(primary); /* TODO: frame index? */
    // begin opaque objs
    
    m_post_processing.RenderPre(engine, frame);

    // begin translucent objs
    m_pass.GetFramebuffer(frame_index)->BeginCapture(primary);
    /* Render deferred shading onto full screen quad */
    HYPERION_ASSERT_RESULT(m_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, frame);
    m_pass.GetFramebuffer(frame_index)->EndCapture(primary);
    // end translucent objs

    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

} // namespace hyperion::v2
