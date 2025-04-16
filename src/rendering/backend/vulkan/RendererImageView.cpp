/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererImageView.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>


#include <core/debug/Debug.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

#pragma region VulkanImageView

VulkanImageView::VulkanImageView()
    : m_handle(VK_NULL_HANDLE),
      m_num_faces(1)
{
}

VulkanImageView::~VulkanImageView()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "image view should have been destroyed");
}

bool VulkanImageView::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanImageView::Create(const ImageBase *image)
{
    return Create(
        image,
        0,
        image->NumMipmaps(),
        0,
        image->NumFaces()
    );
}

RendererResult VulkanImageView::Create(
    const ImageBase *image,
    uint32 mipmap_layer,
    uint32 num_mipmaps,
    uint32 face_layer,
    uint32 num_faces
)
{
    AssertThrow(image != nullptr);
    AssertThrow(static_cast<const VulkanImage *>(image)->GetVulkanHandle() != VK_NULL_HANDLE);

    m_num_faces = num_faces;

    VkImageViewCreateInfo view_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_info.image = static_cast<const VulkanImage *>(image)->GetVulkanHandle();
    view_info.viewType = helpers::ToVkImageViewType(image->GetType(), image->IsTextureArray());
    view_info.format =  helpers::ToVkFormat(image->GetTextureFormat());

    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    view_info.subresourceRange.aspectMask = helpers::ToVkImageAspect(image->GetTextureFormat());
    view_info.subresourceRange.baseMipLevel = mipmap_layer;
    view_info.subresourceRange.levelCount = num_mipmaps;
    view_info.subresourceRange.baseArrayLayer = face_layer;
    view_info.subresourceRange.layerCount = num_faces;

    // AssertThrowMsg(mipmap_layer < num_mipmaps, "mipmap layer out of bounds");
    // AssertThrowMsg(face_layer < m_num_faces, "face layer out of bounds");

    HYPERION_VK_CHECK_MSG(
        vkCreateImageView(GetRenderingAPI()->GetDevice()->GetDevice(), &view_info, nullptr, &m_handle),
        "Failed to create image view"
    );

    HYPERION_RETURN_OK;
}

RendererResult VulkanImageView::Destroy()
{
    if (m_handle != VK_NULL_HANDLE) {
        vkDestroyImageView(GetRenderingAPI()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

#pragma endregion VulkanImageView

} // namespace renderer
} // namespace hyperion