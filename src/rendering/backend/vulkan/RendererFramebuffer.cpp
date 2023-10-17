#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <math/MathUtil.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

FramebufferObject<Platform::VULKAN>::FramebufferObject(Extent2D extent)
    : FramebufferObject(Extent3D(extent))
{
}

FramebufferObject<Platform::VULKAN>::FramebufferObject(Extent3D extent)
    : m_extent(extent),
      m_handle(VK_NULL_HANDLE)
{
}

FramebufferObject<Platform::VULKAN>::~FramebufferObject()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "handle should have been destroyed");
}

Result FramebufferObject<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, RenderPass<Platform::VULKAN> *render_pass)
{
    Array<VkImageView> attachment_image_views;
    attachment_image_views.Reserve(m_attachment_usages.size());

    UInt num_layers = 1;
    
    for (auto *attachment_usage : m_attachment_usages) {
        AssertThrow(attachment_usage != nullptr);
        AssertThrow(attachment_usage->GetImageView() != nullptr);
        AssertThrow(attachment_usage->GetImageView()->GetImageView() != nullptr);

        //num_layers = MathUtil::Max(num_layers, attachment_usage->GetImageView()->NumFaces());

        attachment_image_views.PushBack(attachment_usage->GetImageView()->GetImageView());
    }

    VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass      = render_pass->GetHandle();
    framebuffer_create_info.attachmentCount = UInt32(attachment_image_views.Size());
    framebuffer_create_info.pAttachments    = attachment_image_views.Data();
    framebuffer_create_info.width           = m_extent.width;
    framebuffer_create_info.height          = m_extent.height;
    framebuffer_create_info.layers          = num_layers;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result FramebufferObject<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    auto result = Result::OK;

    vkDestroyFramebuffer(device->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;
    
    for (auto *attachment_usage : m_attachment_usages) {
        attachment_usage->DecRef(HYP_ATTACHMENT_USAGE_INSTANCE);
    }

    m_attachment_usages.clear();

    return result;
}

void FramebufferObject<Platform::VULKAN>::AddAttachmentUsage(AttachmentUsage *attachment_usage)
{
    attachment_usage->IncRef(HYP_ATTACHMENT_USAGE_INSTANCE);

    m_attachment_usages.push_back(attachment_usage);
}

bool FramebufferObject<Platform::VULKAN>::RemoveAttachmentUsage(const Attachment *attachment)
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

    (*it)->DecRef(HYP_ATTACHMENT_USAGE_INSTANCE);

    m_attachment_usages.erase(it);

    return true;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
