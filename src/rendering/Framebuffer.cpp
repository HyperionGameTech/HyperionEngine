#include "Framebuffer.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

Framebuffer::Framebuffer(
    Extent2D extent,
    Handle<RenderPass> &&render_pass
) : EngineComponentBase(),
    m_framebuffer(extent),
    m_render_pass(std::move(render_pass))
{
}

Framebuffer::~Framebuffer()
{
    Teardown();
}

void Framebuffer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    engine->InitObject(m_render_pass);
    
    OnInit(engine->callbacks.Once(EngineCallback::CREATE_FRAMEBUFFERS, [this](...) {
        auto *engine = GetEngine();

        AssertThrowMsg(m_render_pass != nullptr, "Render pass must be set on framebuffer.");

        engine->render_scheduler.Enqueue([this, engine](...) {
            return m_framebuffer.Create(engine->GetDevice(), &m_render_pass->GetRenderPass());
        });

        SetReady(true);

        OnTeardown([this]() {
            auto *engine = GetEngine();

            SetReady(false);

            engine->render_scheduler.Enqueue([this, engine](...) {
                return m_framebuffer.Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

void Framebuffer::AddAttachmentRef(AttachmentRef *attachment_ref)
{
    m_framebuffer.AddAttachmentRef(attachment_ref);    
}

void Framebuffer::RemoveAttachmentRef(const Attachment *attachment)
{
    m_framebuffer.RemoveAttachmentRef(attachment);
}

void Framebuffer::BeginCapture(CommandBuffer *command_buffer)
{
    m_render_pass->GetRenderPass().Begin(command_buffer, &m_framebuffer);
}

void Framebuffer::EndCapture(CommandBuffer *command_buffer)
{
    m_render_pass->GetRenderPass().End(command_buffer);
}

} // namespace hyperion::v2
