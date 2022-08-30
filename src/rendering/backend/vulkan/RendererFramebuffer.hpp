#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <Types.hpp>

#include <memory>
#include <vector>

namespace hyperion {
namespace renderer {

class FramebufferObject
{
public:
    FramebufferObject(Extent2D extent);
    FramebufferObject(Extent3D extent);
    FramebufferObject(const FramebufferObject &other) = delete;
    FramebufferObject &operator=(const FramebufferObject &other) = delete;
    ~FramebufferObject();

    VkFramebuffer GetHandle() const { return m_handle; }

    Result Create(Device *device, RenderPass *render_pass);
    Result Destroy(Device *device);

    void AddAttachmentRef(AttachmentRef *attachment_ref);
    bool RemoveAttachmentRef(const Attachment *attachment);

    auto &GetAttachmentRefs() { return m_attachment_refs; }
    const auto &GetAttachmentRefs() const { return m_attachment_refs; }

    UInt GetWidth() const { return m_extent.width; }
    UInt GetHeight() const { return m_extent.height; }

private:
    Extent3D m_extent;

    std::vector<AttachmentRef *> m_attachment_refs;

    VkFramebuffer m_handle;
};

} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_H
