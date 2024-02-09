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
        uint mipmap_layer,
        uint num_mipmaps,
        uint face_layer,
        uint num_faces
    );

    uint NumFaces() const { return m_num_faces; }

    /* Create imageview referencing an Image */
    Result Create(
        Device<Platform::VULKAN> *device,
        const Image<Platform::VULKAN> *image,
        uint mipmap_layer,
        uint num_mipmaps,
        uint face_layer,
        uint num_faces
    );

    /* Create imageview referencing an Image */
    Result Create(
        Device<Platform::VULKAN> *device,
        const Image<Platform::VULKAN> *image
    );

    Result Destroy(Device<Platform::VULKAN> *device);

private:
    VkImageView m_image_view;

    uint        m_num_faces;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif