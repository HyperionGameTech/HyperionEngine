#ifndef HYPERION_RENDERER_IMAGE_VIEW_H
#define HYPERION_RENDERER_IMAGE_VIEW_H

#include "RendererResult.hpp"
#include "RendererImage.hpp"

#include <vulkan/vulkan.h>

#include <optional>

namespace hyperion {
namespace renderer {
class Device;
class ImageView {
public:
    ImageView();
    ImageView(VkImage image);
    ImageView(const ImageView &other) = delete;
    ImageView &operator=(const ImageView &other) = delete;
    ~ImageView();

    inline VkImageView &GetImageView()             { return m_image_view; }
    inline const VkImageView &GetImageView() const { return m_image_view; }

    /* Create imageview independent of an Image */
    Result Create(
        Device *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        uint32_t num_mipmaps = 1,
        uint32_t num_faces = 1
    );

    uint32_t NumFaces() const { return m_num_faces; }

    /* Create imageview referencing an Image */
    Result Create(Device *device, Image *image);
    Result Destroy(Device *device);

private:
    static VkImageAspectFlags ToVkImageAspect(Image::InternalFormat);
    static VkImageViewType ToVkImageViewType(Image::Type);
    
    VkImageView m_image_view;
    std::optional<VkImage> m_image;

    uint32_t m_num_faces;
};

} // namespace renderer
} // namespace hyperion

#endif