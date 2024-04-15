/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

ImageView<Platform::VULKAN>::ImageView()
    : m_image_view(VK_NULL_HANDLE),
      m_num_faces(1)
{
}

ImageView<Platform::VULKAN>::ImageView(ImageView<Platform::VULKAN> &&other) noexcept
    : m_image_view(other.m_image_view),
      m_num_faces(other.m_num_faces)
{
    other.m_image_view = VK_NULL_HANDLE;
    other.m_num_faces = 1;
}

ImageView<Platform::VULKAN> &ImageView<Platform::VULKAN>::operator=(ImageView<Platform::VULKAN> &&other) noexcept
{
    m_image_view = other.m_image_view;
    m_num_faces = other.m_num_faces;

    other.m_image_view = VK_NULL_HANDLE;
    other.m_num_faces = 1;

    return *this;
}

ImageView<Platform::VULKAN>::~ImageView()
{
    AssertThrowMsg(m_image_view == VK_NULL_HANDLE, "image view should have been destroyed");
}

Result ImageView<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    uint mipmap_layer,
    uint num_mipmaps,
    uint face_layer,
    uint num_faces
)
{
    m_num_faces = num_faces;

    VkImageViewCreateInfo view_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_info.image = image;
    view_info.viewType = view_type;
    view_info.format = format;

    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = mipmap_layer;
    view_info.subresourceRange.levelCount = num_mipmaps;
    view_info.subresourceRange.baseArrayLayer = face_layer;
    view_info.subresourceRange.layerCount = m_num_faces;

    // AssertThrowMsg(mipmap_layer < num_mipmaps, "mipmap layer out of bounds");
    // AssertThrowMsg(face_layer < m_num_faces, "face layer out of bounds");

    HYPERION_VK_CHECK_MSG(
        vkCreateImageView(device->GetDevice(), &view_info, nullptr, &m_image_view),
        "Failed to create image view"
    );

    HYPERION_RETURN_OK;
}

Result ImageView<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    const Image<Platform::VULKAN> *image,
    uint mipmap_layer,
    uint num_mipmaps,
    uint face_layer,
    uint num_faces
)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    return Create(
        device,
        image->GetGPUImage()->image,
        helpers::ToVkFormat(image->GetTextureFormat()),
        helpers::ToVkImageAspect(image->GetTextureFormat()),
        helpers::ToVkImageViewType(image->GetType(), image->IsTextureArray()),
        mipmap_layer,
        num_mipmaps,
        face_layer,
        num_faces
    );
}

Result ImageView<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    const Image<Platform::VULKAN> *image
)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    return Create(
        device,
        image->GetGPUImage()->image,
        helpers::ToVkFormat(image->GetTextureFormat()),
        helpers::ToVkImageAspect(image->GetTextureFormat()),
        helpers::ToVkImageViewType(image->GetType(), image->IsTextureArray()),
        0,
        image->NumMipmaps(),
        0,
        image->NumFaces()
    );
}

Result ImageView<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    vkDestroyImageView(device->GetDevice(), m_image_view, nullptr);
    m_image_view = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion