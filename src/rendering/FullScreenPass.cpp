#include "./FullScreenPass.hpp"
#include <Engine.hpp>
#include <Types.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::FillMode;

struct RENDER_COMMAND(CreateCommandBuffers) : RenderCommandBase2
{
    UniquePtr<renderer::CommandBuffer> *command_buffers;

    RENDER_COMMAND(CreateCommandBuffers)(UniquePtr<renderer::CommandBuffer> *command_buffers)
        : command_buffers(command_buffers)
    {
    }

    virtual Result operator()()
    {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_BUBBLE_ERRORS(command_buffers[i]->Create(
                Engine::Get()->GetDevice(),
                Engine::Get()->GetInstance()->GetGraphicsCommandPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyFullScreenPassAttachments) : RenderCommandBase2
{
    Array<std::unique_ptr<renderer::Attachment>> attachments;

    RENDER_COMMAND(DestroyFullScreenPassAttachments)(Array<std::unique_ptr<renderer::Attachment>> &&attachments)
        : attachments(std::move(attachments))
    {
    }

    virtual Result operator()()
    {
        auto result = renderer::Result::OK;

        for (auto &attachment : attachments) {
            HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetInstance()->GetDevice()), result);
        }

        attachments.Clear();

        return result;
    }
};

FullScreenPass::FullScreenPass(InternalFormat image_format)
    : FullScreenPass(Handle<Shader>(), image_format)
{
}

FullScreenPass::FullScreenPass(
    Handle<Shader> &&shader,
    InternalFormat image_format
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
    InternalFormat image_format
) : m_shader(std::move(shader)),
    m_descriptor_key(descriptor_key),
    m_sub_descriptor_index(sub_descriptor_index),
    m_image_format(image_format)
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::Create()
{
    Threads::AssertOnThread(THREAD_RENDER);

    Engine::Get()->InitObject(m_shader);

    CreateQuad(Engine::Get());
    CreateRenderPass(Engine::Get());
    CreateCommandBuffers(Engine::Get());
    CreateFramebuffers(Engine::Get());
    CreatePipeline(Engine::Get());
    CreateDescriptors(Engine::Get());
    
    HYP_FLUSH_RENDER_QUEUE();
}

void FullScreenPass::SetShader(Handle<Shader> &&shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = std::move(shader);
}

void FullScreenPass::CreateQuad()
{
    m_full_screen_quad = Engine::Get()->CreateHandle<Mesh>(MeshBuilder::Quad());
    Engine::Get()->InitObject(m_full_screen_quad);
}

void FullScreenPass::CreateRenderPass()
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    renderer::AttachmentRef *attachment_ref;

    auto framebuffer_image = std::make_unique<renderer::FramebufferImage2D>(
        Engine::Get()->GetInstance()->swapchain->extent,
        m_image_format,
        nullptr
    );

    m_attachments.PushBack(std::make_unique<Attachment>(
        std::move(framebuffer_image),
        renderer::RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentRef(
        Engine::Get()->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetInstance()->GetDevice()));
    }

    m_render_pass = Engine::Get()->CreateHandle<RenderPass>(render_pass.release());
    Engine::Get()->InitObject(m_render_pass);
}

void FullScreenPass::CreateCommandBuffers()
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_command_buffers[i] = UniquePtr<CommandBuffer>::Construct(CommandBuffer::COMMAND_BUFFER_SECONDARY);
    }

    // create command buffers in render thread
    RenderCommands::Push<RENDER_COMMAND(CreateCommandBuffers)>(m_command_buffers.Data());
}

void FullScreenPass::CreateFramebuffers()
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = Engine::Get()->CreateHandle<Framebuffer>(
            Engine::Get()->GetInstance()->swapchain->extent,
            Handle<RenderPass>(m_render_pass)
        );

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }
        
        Engine::Get()->InitObject(m_framebuffers[i]);
    }
}

void FullScreenPass::CreatePipeline()
{
    CreatePipeline(Engine::Get(), RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .fill_mode = FillMode::FILL,
            .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
        }
    ));
}

void FullScreenPass::CreatePipeline( const RenderableAttributeSet &renderable_attributes)
{
    auto _renderer_instance = std::make_unique<RendererInstance>(
        std::move(m_shader),
        Handle<RenderPass>(m_render_pass),
        renderable_attributes
    );

    for (auto &framebuffer : m_framebuffers) {
        _renderer_instance->AddFramebuffer(Handle<Framebuffer>(framebuffer));
    }

    m_renderer_instance = Engine::Get()->AddRendererInstance(std::move(_renderer_instance));
    Engine::Get()->InitObject(m_renderer_instance);
}

void FullScreenPass::Destroy()
{
    Engine::Get()->SafeReleaseHandle<Mesh>(std::move(m_full_screen_quad));

    // TODO: Move all attachment ops into render thread
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        if (m_framebuffers[i] != nullptr) {
            for (auto &attachment : m_attachments) {
                m_framebuffers[i]->RemoveAttachmentRef(attachment.get());
            }

            if (m_renderer_instance != nullptr) {
                m_renderer_instance->RemoveFramebuffer(m_framebuffers[i]->GetID());
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

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        Engine::Get()->SafeRelease(std::move(m_command_buffers[i]));
    }

    RenderCommands::Push<RENDER_COMMAND(DestroyFullScreenPassAttachments)>(std::move(m_attachments));

    HYP_FLUSH_RENDER_QUEUE();
}

void FullScreenPass::Record( UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        Engine::Get()->GetInstance()->GetDevice(),
        m_renderer_instance->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_renderer_instance->GetPipeline()->push_constants = m_push_constant_data;
            m_renderer_instance->GetPipeline()->Bind(cmd);

            const UInt scene_index = Engine::Get()->render_state.GetScene().id.ToIndex();

            cmd->BindDescriptorSet(
                Engine::Get()->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );

            cmd->BindDescriptorSet(
                Engine::Get()->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::scene_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                FixedArray {
                    HYP_RENDER_OBJECT_OFFSET(Scene, scene_index),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0)
                }
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                Engine::Get()->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                Engine::Get()->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif

            cmd->BindDescriptorSet(
                Engine::Get()->GetInstance()->GetDescriptorPool(),
                m_renderer_instance->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER
            );
            
#if HYP_FEATURES_ENABLE_RAYTRACING
          //  if (!Engine::Get()->GetDevice()->GetFeatures().IsRaytracingDisabled()) {
                cmd->BindDescriptorSet(
                    Engine::Get()->GetInstance()->GetDescriptorPool(),
                    m_renderer_instance->GetPipeline(),
                    DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
                    DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING
                );
         //   }
#endif

            m_full_screen_quad->Render(Engine::Get(), cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void FullScreenPass::Render( Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto frame_index = frame->GetFrameIndex();

    m_framebuffers[frame_index]->BeginCapture(frame->GetCommandBuffer());

    auto *secondary_command_buffer = m_command_buffers[frame_index].Get();
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffers[frame_index]->EndCapture(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
