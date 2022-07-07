#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include "Base.hpp"
#include "RenderPass.hpp"

#include <rendering/backend/RendererFbo.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Extent2D;
using renderer::AttachmentRef;
using renderer::Attachment;

class Framebuffer
    : public EngineComponentBase<STUB_CLASS(Framebuffer)>,
      public RenderObject
{
public:
    Framebuffer(Extent2D extent, Ref<RenderPass> &&render_pass);
    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    void AddAttachmentRef(AttachmentRef *attachment_ref);
    void RemoveAttachmentRef(const Attachment *attachment);

    renderer::FramebufferObject &GetFramebuffer()             { return m_framebuffer; }
    const renderer::FramebufferObject &GetFramebuffer() const { return m_framebuffer; }

    RenderPass *GetRenderPass() const { return m_render_pass.ptr; }

    void Init(Engine *engine);

    void BeginCapture(CommandBuffer *command_buffer);
    void EndCapture(CommandBuffer *command_buffer);

private:
    renderer::FramebufferObject m_framebuffer;
    Ref<RenderPass>             m_render_pass;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H
