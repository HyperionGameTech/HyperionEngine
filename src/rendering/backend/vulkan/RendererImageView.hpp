#ifndef HYPERION_RENDERER_IMAGE_VIEW_H
#define HYPERION_RENDERER_IMAGE_VIEW_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <vulkan/vulkan.h>

#include <core/lib/Optional.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

class Device;

class ImageView
{
public:
    ImageView();
    ImageView(VkImage image);
    ImageView(const ImageView &other) = delete;
    ImageView &operator=(const ImageView &other) = delete;
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
    static VkImageAspectFlags ToVkImageAspect(Image::InternalFormat);
    static VkImageViewType ToVkImageViewType(Image::Type);
    
    VkImageView m_image_view;
    Optional<VkImage> m_image;

    UInt m_num_faces;
};

} // namespace renderer
} // namespace hyperion

#endif