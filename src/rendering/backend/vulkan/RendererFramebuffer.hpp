#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include "RendererAttachment.hpp"
#include "RendererImage.hpp"
#include "RendererImageView.hpp"
#include "RendererSampler.hpp"
#include "RendererRenderPass.hpp"

#include <memory>
#include <vector>

namespace hyperion {
namespace renderer {

class FramebufferObject {
public:
    FramebufferObject(Extent2D extent);
    FramebufferObject(const FramebufferObject &other) = delete;
    FramebufferObject &operator=(const FramebufferObject &other) = delete;
    ~FramebufferObject();

    VkFramebuffer GetHandle() const { return m_handle; }

    Result Create(Device *device, RenderPass *render_pass);
    Result Destroy(Device *device);

    void AddAttachmentRef(AttachmentRef *attachment_ref);
    bool RemoveAttachmentRef(const Attachment *attachment);

    auto &GetAttachmentRefs()             { return m_attachment_refs; }
    const auto &GetAttachmentRefs() const { return m_attachment_refs; }

    uint32_t GetWidth()  const            { return m_extent.width; }
    uint32_t GetHeight() const            { return m_extent.height; }

private:
    Extent2D m_extent;

    std::vector<AttachmentRef *> m_attachment_refs;

    VkFramebuffer m_handle;
};

} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_H
