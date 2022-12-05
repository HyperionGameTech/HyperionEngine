#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include <core/Base.hpp>
#include <rendering/RenderPass.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Extent2D;
using renderer::Extent3D;
using renderer::AttachmentRef;
using renderer::Attachment;


class Framebuffer
    : public EngineComponentBase<STUB_CLASS(Framebuffer)>,
      public RenderResource
{
public:
    Framebuffer(
        Extent2D extent,
        renderer::RenderPassStage stage = renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode render_pass_mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE,
        UInt num_multiview_layers = 0
    );

    Framebuffer(
        Extent3D extent,
        renderer::RenderPassStage stage = renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode render_pass_mode = renderer::RenderPass::Mode::RENDER_PASS_INLINE,
        UInt num_multiview_layers = 0
    );

    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    void AddAttachmentRef(AttachmentRef *attachment);
    void RemoveAttachmentRef(const Attachment *attachment);

    auto &GetAttachmentRefs() { return m_render_pass.GetAttachmentRefs(); }
    const auto &GetAttachmentRefs() const { return m_render_pass.GetAttachmentRefs(); }

    renderer::FramebufferObject &GetFramebuffer(UInt frame_index) { return m_framebuffers[frame_index]; }
    const renderer::FramebufferObject &GetFramebuffer(UInt frame_index) const { return m_framebuffers[frame_index]; }

    renderer::RenderPass &GetRenderPass() { return m_render_pass; }
    const renderer::RenderPass &GetRenderPass() const { return m_render_pass; }

    Extent2D GetExtent() const
        { return { m_framebuffers[0].GetWidth(), m_framebuffers[0].GetHeight() }; }

    void Init();

    void BeginCapture(UInt frame_index, CommandBuffer *command_buffer);
    void EndCapture(UInt frame_index, CommandBuffer *command_buffer);

private:
    FixedArray<renderer::FramebufferObject, max_frames_in_flight> m_framebuffers;
    renderer::RenderPass m_render_pass;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H
