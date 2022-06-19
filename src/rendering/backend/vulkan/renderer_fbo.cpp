#include "renderer_fbo.h"
#include "renderer_render_pass.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
FramebufferObject::FramebufferObject(Extent2D extent)
    : m_extent(extent),
      m_handle(VK_NULL_HANDLE)
{
}

FramebufferObject::~FramebufferObject()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "handle should have been destroyed");
}

Result FramebufferObject::Create(Device *device, RenderPass *render_pass)
{
    std::vector<VkImageView> attachment_image_views;
    attachment_image_views.reserve(m_attachment_refs.size());
    
    for (auto *attachment_ref : m_attachment_refs) {
        AssertThrow(attachment_ref != nullptr);
        AssertThrow(attachment_ref->GetImageView() != nullptr);
        AssertThrow(attachment_ref->GetImageView()->GetImageView() != nullptr);

        attachment_image_views.push_back(attachment_ref->GetImageView()->GetImageView());
    }

    VkFramebufferCreateInfo framebuffer_create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_create_info.renderPass      = render_pass->GetHandle();
    framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachment_image_views.size());
    framebuffer_create_info.pAttachments    = attachment_image_views.data();
    framebuffer_create_info.width           = m_extent.width;
    framebuffer_create_info.height          = m_extent.height;
    framebuffer_create_info.layers          = 1;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result FramebufferObject::Destroy(Device *device)
{
    auto result = Result::OK;

    vkDestroyFramebuffer(device->GetDevice(), m_handle, nullptr);
    m_handle = nullptr;
    
    for (auto *attachment_ref : m_attachment_refs) {
        attachment_ref->DecRef(HYP_ATTACHMENT_REF_INSTANCE);
    }

    m_attachment_refs.clear();

    return result;
}

void FramebufferObject::AddAttachmentRef(AttachmentRef *attachment_ref)
{
    attachment_ref->IncRef(HYP_ATTACHMENT_REF_INSTANCE);

    m_attachment_refs.push_back(attachment_ref);
}

bool FramebufferObject::RemoveAttachmentRef(const Attachment *attachment)
{
    const auto it = std::find_if(
        m_attachment_refs.begin(),
        m_attachment_refs.end(),
        [attachment](const AttachmentRef *item) {
            return item->GetAttachment() == attachment;
        }
    );

    if (it == m_attachment_refs.end()) {
        return false;
    }

    (*it)->DecRef(HYP_ATTACHMENT_REF_INSTANCE);

    m_attachment_refs.erase(it);

    return true;
}

} // namespace renderer
} // namespace hyperion
