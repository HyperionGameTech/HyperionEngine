/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <vulkan/vulkan.h>

#include <core/utilities/Optional.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <>
class ImageView<Platform::VULKAN>
{
public:
    HYP_API ImageView();
    ImageView(const ImageView &other)               = delete;
    ImageView &operator=(const ImageView &other)    = delete;
    HYP_API ImageView(ImageView &&other) noexcept;
    HYP_API ImageView &operator=(ImageView &&other) noexcept;
    HYP_API ~ImageView();

    VkImageView GetImageView() const
        { return m_image_view; }

    /* Create imageview independent of an Image */
    HYP_API Result Create(
        Device<Platform::VULKAN> *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        uint mipmap_layer,
        uint num_mipmaps,
        uint face_layer,
        uint num_faces
    );

    uint NumFaces() const
        { return m_num_faces; }

    bool IsCreated() const
        { return m_is_created; }

    /* Create imageview referencing an Image */
    HYP_API Result Create(
        Device<Platform::VULKAN> *device,
        const Image<Platform::VULKAN> *image,
        uint mipmap_layer,
        uint num_mipmaps,
        uint face_layer,
        uint num_faces
    );

    /* Create imageview referencing an Image */
    HYP_API Result Create(
        Device<Platform::VULKAN> *device,
        const Image<Platform::VULKAN> *image
    );

    HYP_API Result Destroy(Device<Platform::VULKAN> *device);

private:
    VkImageView m_image_view;

    uint        m_num_faces;

    bool        m_is_created;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif