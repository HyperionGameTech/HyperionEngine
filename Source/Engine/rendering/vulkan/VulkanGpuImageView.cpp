/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/debug/Debug.hpp>

#include <VulkanGpuImageView.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

#pragma region VulkanGpuImageView

VulkanGpuImageView::VulkanGpuImageView(const VulkanGpuImageRef& image)
    : GpuImageViewBase(image),
      m_handle(VK_NULL_HANDLE),
      m_viewType(VK_IMAGE_VIEW_TYPE_MAX_ENUM)
{
}

VulkanGpuImageView::VulkanGpuImageView(
    const VulkanGpuImageRef& image,
    const ImageSubResource& subResource)
    : GpuImageViewBase(image, subResource),
      m_handle(VK_NULL_HANDLE),
      m_viewType(VK_IMAGE_VIEW_TYPE_MAX_ENUM)
{
}

VulkanGpuImageView::VulkanGpuImageView(
    const VulkanGpuImageRef& image,
    const ImageSubResource& subResource,
    VkImageViewType viewType)
    : GpuImageViewBase(image, subResource),
      m_handle(VK_NULL_HANDLE),
      m_viewType(viewType)
{
}

VulkanGpuImageView::~VulkanGpuImageView()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]() -> void
            {
                vkDestroyImageView(RI.GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }
}

bool VulkanGpuImageView::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanGpuImageView::Create()
{
    if (IsCreated())
    {
        return {}; // already created
    }

    if (!m_image)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create image view on uninitialized image");
    }

    if (m_subResource.baseArrayLayer >= m_image->NumArrayLayers())
    {
        return HYP_MAKE_ERROR(RendererError, "Face index out of bounds");
    }

    if (m_subResource.baseMipLevel >= m_image->NumMips())
    {
        return HYP_MAKE_ERROR(RendererError, "Mip index out of bounds");
    }

    Assert(static_cast<const VulkanGpuImage*>(m_image.Get())->GetVulkanHandle() != VK_NULL_HANDLE);

    if (m_viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM)
    {
        m_viewType = ToVkImageViewType(m_image->GetType());

        // When viewing a single face of a cubemap as a 2D texture, use VIEW_TYPE_2D
        const TextureType imageType = m_image->GetType();
        if ((imageType == TextureType::Cubemap || imageType == TextureType::CubemapArray)
            && m_subResource.numLayers == 1)
        {
            m_viewType = VK_IMAGE_VIEW_TYPE_2D;
        }
    }

    VkImageViewCreateInfo viewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = static_cast<const VulkanGpuImage*>(m_image.Get())->GetVulkanHandle();
    viewInfo.viewType = m_viewType;
    viewInfo.format = ToVkFormat(m_image->GetTextureFormat());

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(m_image->GetTextureFormat());
    viewInfo.subresourceRange.baseMipLevel = m_subResource.baseMipLevel;
    viewInfo.subresourceRange.levelCount = m_subResource.numLevels != 0 ? m_subResource.numLevels : m_image->NumMips();
    viewInfo.subresourceRange.baseArrayLayer = m_subResource.baseArrayLayer;
    viewInfo.subresourceRange.layerCount = m_subResource.numLayers != 0 ? m_subResource.numLayers : m_image->NumArrayLayers();

    VULKAN_CHECK_MSG(
        vkCreateImageView(RI.GetDevice()->GetDevice(), &viewInfo, nullptr, &m_handle),
        "Failed to create image view");

#if HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    return {};
}

#if HYP_DEBUG_MODE

void VulkanGpuImageView::SetDebugName(Name name)
{
    GpuImageViewBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuImageView

} // namespace Hyperion
