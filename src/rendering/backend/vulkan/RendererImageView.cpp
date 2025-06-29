/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererImageView.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderBackend* g_render_backend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

#pragma region VulkanImageView

VulkanImageView::VulkanImageView(const VulkanImageRef& image)
    : ImageViewBase(image),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanImageView::VulkanImageView(
    const VulkanImageRef& image,
    uint32 mip_index,
    uint32 num_mips,
    uint32 face_index,
    uint32 num_faces)
    : ImageViewBase(image, mip_index, num_mips, face_index, num_faces),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanImageView::~VulkanImageView()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "image view should have been destroyed");

    SafeRelease(std::move(m_image));
}

bool VulkanImageView::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanImageView::Create()
{
    if (!m_image)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create image view on uninitialized image");
    }

    if (m_face_index >= m_image->NumFaces())
    {
        return HYP_MAKE_ERROR(RendererError, "Face index out of bounds");
    }

    if (m_mip_index >= m_image->NumMipmaps())
    {
        return HYP_MAKE_ERROR(RendererError, "Mip index out of bounds");
    }

    AssertThrow(static_cast<const VulkanImage*>(m_image.Get())->GetVulkanHandle() != VK_NULL_HANDLE);

    VkImageViewCreateInfo view_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_info.image = static_cast<const VulkanImage*>(m_image.Get())->GetVulkanHandle();
    view_info.viewType = helpers::ToVkImageViewType(m_image->GetType());
    view_info.format = helpers::ToVkFormat(m_image->GetTextureFormat());

    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    view_info.subresourceRange.aspectMask = helpers::ToVkImageAspect(m_image->GetTextureFormat());
    view_info.subresourceRange.baseMipLevel = m_mip_index;
    view_info.subresourceRange.levelCount = m_num_mips != 0 ? m_num_mips : m_image->NumMipmaps();
    view_info.subresourceRange.baseArrayLayer = m_face_index;
    view_info.subresourceRange.layerCount = m_num_faces != 0 ? m_num_faces : m_image->NumFaces();

    // AssertThrowMsg(mipmap_layer < num_mipmaps, "mipmap layer out of bounds");
    // AssertThrowMsg(face_layer < m_num_faces, "face layer out of bounds");

    HYPERION_VK_CHECK_MSG(
        vkCreateImageView(GetRenderBackend()->GetDevice()->GetDevice(), &view_info, nullptr, &m_handle),
        "Failed to create image view");

    HYPERION_RETURN_OK;
}

RendererResult VulkanImageView::Destroy()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyImageView(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);

        m_handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

#pragma endregion VulkanImageView

} // namespace hyperion