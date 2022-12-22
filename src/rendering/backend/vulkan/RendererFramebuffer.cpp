#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <math/MathUtil.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
FramebufferObject::FramebufferObject(Extent2D extent)
    : FramebufferObject(Extent3D(extent))
{
}

FramebufferObject::FramebufferObject(Extent3D extent)
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
    attachment_image_views.reserve(m_attachment_usages.size());

    UInt num_layers = 1;
    
    for (auto *attachment_usage : m_attachment_usages) {
        AssertThrow(attachment_usage != nullptr);
        AssertThrow(attachment_usage->GetImageView() != nullptr);
        AssertThrow(attachment_usage->GetImageView()->GetImageView() != nullptr);

        //num_layers = MathUtil::Max(num_layers, attachment_usage->GetImageView()->NumFaces());

        attachment_image_views.push_back(attachment_usage->GetImageView()->GetImageView());
    }

    VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass = render_pass->GetHandle();
    framebuffer_create_info.attachmentCount = static_cast<UInt32>(attachment_image_views.size());
    framebuffer_create_info.pAttachments = attachment_image_views.data();
    framebuffer_create_info.width = m_extent.width;
    framebuffer_create_info.height = m_extent.height;
    framebuffer_create_info.layers = num_layers;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result FramebufferObject::Destroy(Device *device)
{
    auto result = Result::OK;

    vkDestroyFramebuffer(device->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;
    
    for (auto *attachment_usage : m_attachment_usages) {
        attachment_usage->DecRef(HYP_attachment_usage_INSTANCE);
    }

    m_attachment_usages.clear();

    return result;
}

void FramebufferObject::AddAttachmentUsage(AttachmentUsage *attachment_usage)
{
    attachment_usage->IncRef(HYP_attachment_usage_INSTANCE);

    m_attachment_usages.push_back(attachment_usage);
}

bool FramebufferObject::RemoveAttachmentUsage(const Attachment *attachment)
{
    const auto it = std::find_if(
        m_attachment_usages.begin(),
        m_attachment_usages.end(),
        [attachment](const AttachmentUsage *item) {
            return item->GetAttachment() == attachment;
        }
    );

    if (it == m_attachment_usages.end()) {
        return false;
    }

    (*it)->DecRef(HYP_attachment_usage_INSTANCE);

    m_attachment_usages.erase(it);

    return true;
}

} // namespace renderer
} // namespace hyperion
