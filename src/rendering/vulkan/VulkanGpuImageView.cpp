/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

#pragma region VulkanGpuImageView

VulkanGpuImageView::VulkanGpuImageView(const VulkanGpuImageRef& image)
    : GpuImageViewBase(image),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanGpuImageView::VulkanGpuImageView(
    const VulkanGpuImageRef& image,
    uint32 mipIndex,
    uint32 numMips,
    uint32 faceIndex,
    uint32 numFaces)
    : GpuImageViewBase(image, mipIndex, numMips, faceIndex, numFaces),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanGpuImageView::~VulkanGpuImageView()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyImageView(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);

        m_handle = VK_NULL_HANDLE;
    }

    SafeDelete(std::move(m_image));
}

bool VulkanGpuImageView::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanGpuImageView::Create()
{
    if (!m_image)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create image view on uninitialized image");
    }

    if (m_faceIndex >= m_image->NumFaces())
    {
        return HYP_MAKE_ERROR(RendererError, "Face index out of bounds");
    }

    if (m_mipIndex >= m_image->NumMipmaps())
    {
        return HYP_MAKE_ERROR(RendererError, "Mip index out of bounds");
    }

    HYP_GFX_ASSERT(static_cast<const VulkanGpuImage*>(m_image.Get())->GetVulkanHandle() != VK_NULL_HANDLE);

    VkImageViewCreateInfo viewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = static_cast<const VulkanGpuImage*>(m_image.Get())->GetVulkanHandle();
    viewInfo.viewType = ToVkImageViewType(m_image->GetType());
    viewInfo.format = ToVkFormat(m_image->GetTextureFormat());

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(m_image->GetTextureFormat());
    viewInfo.subresourceRange.baseMipLevel = m_mipIndex;
    viewInfo.subresourceRange.levelCount = m_numMips != 0 ? m_numMips : m_image->NumMipmaps();
    viewInfo.subresourceRange.baseArrayLayer = m_faceIndex;
    viewInfo.subresourceRange.layerCount = m_numFaces != 0 ? m_numFaces : m_image->NumFaces();

    // HYP_GFX_ASSERT(mipmapLayer < numMipmaps, "mipmap layer out of bounds");
    // HYP_GFX_ASSERT(faceLayer < m_numFaces, "face layer out of bounds");

    VULKAN_CHECK_MSG(
        vkCreateImageView(GetRenderBackend()->GetDevice()->GetDevice(), &viewInfo, nullptr, &m_handle),
        "Failed to create image view");

    HYPERION_RETURN_OK;
}

#ifdef HYP_DEBUG_MODE

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

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(GetRenderBackend()->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuImageView

} // namespace hyperion
