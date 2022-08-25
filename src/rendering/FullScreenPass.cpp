#include "./FullScreenPass.hpp"
#include <Engine.hpp>
#include <Types.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <builders/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::FillMode;

FullScreenPass::FullScreenPass(Image::InternalFormat image_format)
    : FullScreenPass(Handle<Shader>(), image_format)
{
}

FullScreenPass::FullScreenPass(
    Handle<Shader> &&shader,
    Image::InternalFormat image_format
) : FullScreenPass(std::move(shader),
        DescriptorKey::POST_FX_PRE_STACK,
        ~0u,
        image_format
    )
{
}

FullScreenPass::FullScreenPass(
    Handle<Shader> &&shader,
    DescriptorKey descriptor_key,
    UInt sub_descriptor_index,
    Image::InternalFormat image_format
) : m_shader(std::move(shader)),
    m_descriptor_key(descriptor_key),
    m_sub_descriptor_index(sub_descriptor_index),
    m_image_format(image_format)
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    CreateQuad(engine);
    CreateRenderPass(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->resources->framebuffers.Add(new Framebuffer(
            engine->GetInstance()->swapchain->extent,
            Handle<RenderPass>(m_render_pass)
        ));

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }
        
        m_framebuffers[i].Init();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }

    if (m_shader) {
        m_shader->Init(engine);
    }

    CreatePipeline(engine);
    CreateDescriptors(engine);
    
    HYP_FLUSH_RENDER_QUEUE(engine);
}

void FullScreenPass::SetShader(Handle<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = std::move(shader);

    // m_shader.Init();
}

void FullScreenPass::CreateQuad(Engine *engine)
{
    m_full_screen_quad = Handle<Mesh>(MeshBuilder::Quad().release());
    m_full_screen_quad->Init(engine);
}

void FullScreenPass::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    renderer::AttachmentRef *attachment_ref;

    auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
        engine->GetInstance()->swapchain->extent,
        m_image_format,
        nullptr
    );

    m_attachments.PushBack(std::make_unique<Attachment>(
        std::move(framebuffer_image),
        renderer::RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass = Handle<RenderPass>(render_pass.release());
    m_render_pass->Init(engine);
}

void FullScreenPass::CreateDescriptors(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();
    
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageDescriptor>(m_descriptor_key);

            AssertThrowMsg(framebuffer.GetAttachmentRefs().size() == 1, "> 1 attachments not supported currently for full screen passes");

            for (const auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                m_sub_descriptor_index = descriptor->SetSubDescriptor({
                    .element_index = m_sub_descriptor_index,
                    .image_view    = attachment_ref->GetImageView()
                });
            }
        }
    }
}

void FullScreenPass::CreatePipeline(Engine *engine)
{
    CreatePipeline(engine, RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes,
            .fill_mode = FillMode::FILL,
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
        }
    ));
}

void FullScreenPass::CreatePipeline(Engine *engine, const RenderableAttributeSet &renderable_attributes)
{
    auto _renderer_instance = std::make_unique<RendererInstance>(
        std::move(m_shader),
        Handle<RenderPass>(m_render_pass),
        renderable_attributes
    );

    for (auto &framebuffer : m_framebuffers) {
        _renderer_instance->AddFramebuffer(framebuffer.IncRef());
    }

    m_renderer_instance = engine->AddRendererInstance(std::move(_renderer_instance));
    m_renderer_instance->Init(engine);
}

void FullScreenPass::Destroy(Engine *engine)
{
    engine->SafeReleaseRenderResource<Mesh>(std::move(m_full_screen_quad));

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        if (m_framebuffers[i] != nullptr) {
            for (auto &attachment : m_attachments) {
                m_framebuffers[i]->RemoveAttachmentRef(attachment.get());
            }

            if (m_renderer_instance != nullptr) {
                m_renderer_instance->RemoveFramebuffer(m_framebuffers[i]->GetId());
            }
        }
    }

    if (m_render_pass != nullptr) {
        for (auto &attachment : m_attachments) {
            m_render_pass->GetRenderPass().RemoveAttachmentRef(attachment.get());
        }
    }

    m_framebuffers = {};
    m_render_pass.Reset();
    m_renderer_instance.Reset();

    engine->render_scheduler.Enqueue([this, engine](...) {
        auto result = renderer::Result::OK;

        for (UInt32 i = 0; i < max_frames_in_flight; i++) {
            HYPERION_PASS_ERRORS(
                m_command_buffers[i]->Destroy(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool()
                ),
                result
            );
        }

        m_command_buffers = {};

        for (auto &attachment : m_attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
        }

        m_attachments.Clear();

        return result;
    });

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void FullScreenPass::Record(Engine *engine, UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

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

            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::scene_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                FixedArray {
                    static_cast<UInt32>(sizeof(SceneShaderData) * scene_index),
                    static_cast<UInt32>(sizeof(LightShaderData) * 0)
                }
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
            cmd->BindDescriptorSet(
                engine->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER
            );

            m_full_screen_quad->Render(engine, cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void FullScreenPass::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto frame_index = frame->GetFrameIndex();

    m_framebuffers[frame_index]->BeginCapture(frame->GetCommandBuffer());

    auto *secondary_command_buffer = m_command_buffers[frame_index].get();
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffers[frame_index]->EndCapture(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
