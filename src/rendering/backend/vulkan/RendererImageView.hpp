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
} // namespace platform

using Device = platform::Device<Platform::VULKAN>;

class ImageView
{
public:
    ImageView();
    ImageView(const ImageView &other) = delete;
    ImageView &operator=(const ImageView &other) = delete;
    ImageView(ImageView &&other) noexcept;
    ImageView &operator=(ImageView &&other) noexcept;
    ~ImageView();

    VkImageView &GetImageView() { return m_image_view; }
    const VkImageView &GetImageView() const { return m_image_view; }

    /* Create imageview independent of an Image */
    Result Create(
        Device *device,
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
        Device *device,
        const Image *image,
        UInt mipmap_layer,
        UInt num_mipmaps,
        UInt face_layer,
        UInt num_faces
    );

    /* Create imageview referencing an Image */
    Result Create(
        Device *device,
        const Image *image
    );

    Result Destroy(Device *device);

private:
    VkImageView m_image_view;

    UInt m_num_faces;
};

} // namespace renderer
} // namespace hyperion

#endif