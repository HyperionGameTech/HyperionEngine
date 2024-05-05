/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/system/Debug.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region ImageViewPlatformImpl

Result ImageViewPlatformImpl<Platform::VULKAN>::Create(
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
    self->m_num_faces = num_faces;

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
    view_info.subresourceRange.layerCount = num_faces;

    // AssertThrowMsg(mipmap_layer < num_mipmaps, "mipmap layer out of bounds");
    // AssertThrowMsg(face_layer < m_num_faces, "face layer out of bounds");

    HYPERION_VK_CHECK_MSG(
        vkCreateImageView(device->GetDevice(), &view_info, nullptr, &handle),
        "Failed to create image view"
    );

    HYPERION_RETURN_OK;
}

Result ImageViewPlatformImpl<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (handle != VK_NULL_HANDLE) {
        vkDestroyImageView(device->GetDevice(), handle, nullptr);
        handle = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

#pragma endregion ImageViewPlatformImpl

#pragma region ImageView

template <>
ImageView<Platform::VULKAN>::ImageView()
    : m_platform_impl { this },
      m_num_faces(1)
{
}

template <>
ImageView<Platform::VULKAN>::ImageView(ImageView<Platform::VULKAN> &&other) noexcept
    : m_num_faces(other.m_num_faces)
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;
    other.m_platform_impl = { &other };

    other.m_num_faces = 1;
}

template <>
ImageView<Platform::VULKAN> &ImageView<Platform::VULKAN>::operator=(ImageView<Platform::VULKAN> &&other) noexcept
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;
    other.m_platform_impl = { &other };

    m_num_faces = other.m_num_faces;

    other.m_num_faces = 1;

    return *this;
}

template <>
ImageView<Platform::VULKAN>::~ImageView()
{
    AssertThrowMsg(m_platform_impl.handle == VK_NULL_HANDLE, "image view should have been destroyed");
}

template <>
bool ImageView<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handle != VK_NULL_HANDLE;
}

template <>
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
    AssertThrow(image->GetPlatformImpl().handle != VK_NULL_HANDLE);

    return m_platform_impl.Create(
        device,
        image->GetPlatformImpl().handle,
        helpers::ToVkFormat(image->GetTextureFormat()),
        helpers::ToVkImageAspect(image->GetTextureFormat()),
        helpers::ToVkImageViewType(image->GetType(), image->IsTextureArray()),
        mipmap_layer,
        num_mipmaps,
        face_layer,
        num_faces
    );
}

template <>
Result ImageView<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    const Image<Platform::VULKAN> *image
)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetPlatformImpl().handle != VK_NULL_HANDLE);

    return m_platform_impl.Create(
        device,
        image->GetPlatformImpl().handle,
        helpers::ToVkFormat(image->GetTextureFormat()),
        helpers::ToVkImageAspect(image->GetTextureFormat()),
        helpers::ToVkImageViewType(image->GetType(), image->IsTextureArray()),
        0,
        image->NumMipmaps(),
        0,
        image->NumFaces()
    );
}

template <>
Result ImageView<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    return m_platform_impl.Destroy(device);
}

#pragma endregion ImageView

} // namespace platform
} // namespace renderer
} // namespace hyperion