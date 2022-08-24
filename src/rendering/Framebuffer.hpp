#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include "Base.hpp"
#include "RenderPass.hpp"

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Extent2D;
using renderer::AttachmentRef;
using renderer::Attachment;

class Framebuffer
    : public EngineComponentBase<STUB_CLASS(Framebuffer)>,
      public RenderResource
{
public:
    Framebuffer(Extent2D extent, Handle<RenderPass> &&render_pass);
    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    void AddAttachmentRef(AttachmentRef *attachment);
    void RemoveAttachmentRef(const Attachment *attachment);

    renderer::FramebufferObject &GetFramebuffer() { return m_framebuffer; }
    const renderer::FramebufferObject &GetFramebuffer() const { return m_framebuffer; }

    Handle<RenderPass> &GetRenderPass() { return m_render_pass; }
    const Handle<RenderPass> &GetRenderPass() const { return m_render_pass; }

    void Init(Engine *engine);

    void BeginCapture(CommandBuffer *command_buffer);
    void EndCapture(CommandBuffer *command_buffer);

private:
    renderer::FramebufferObject m_framebuffer;
    Handle<RenderPass> m_render_pass;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H
