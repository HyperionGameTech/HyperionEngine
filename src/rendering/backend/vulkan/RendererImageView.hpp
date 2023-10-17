#ifndef HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <vulkan/vulkan.h>

#include <core/lib/Optional.hpp>
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
    ImageView();
    ImageView(const ImageView &other) = delete;
    ImageView &operator=(const ImageView &other) = delete;
    ImageView(ImageView &&other) noexcept;
    ImageView &operator=(ImageView &&other) noexcept;
    ~ImageView();

    VkImageView GetImageView() const
        { return m_image_view; }

    /* Create imageview independent of an Image */
    Result Create(
        Device<Platform::VULKAN> *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        UInt mipmap_layer,
        UInt num_mipmaps,
        UInt face_layer,
        UInt num_faces
    );

    UInt NumFaces() const { return m_num_faces; }

    /* Create imageview referencing an Image */
    Result Create(
        Device<Platform::VULKAN> *device,
        const Image<Platform::VULKAN> *image,
        UInt mipmap_layer,
        UInt num_mipmaps,
        UInt face_layer,
        UInt num_faces
    );

    /* Create imageview referencing an Image */
    Result Create(
        Device<Platform::VULKAN> *device,
        const Image<Platform::VULKAN> *image
    );

    Result Destroy(Device<Platform::VULKAN> *device);

private:
    VkImageView m_image_view;

    UInt        m_num_faces;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif