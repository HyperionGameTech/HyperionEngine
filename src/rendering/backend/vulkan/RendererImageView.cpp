#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {

ImageView::ImageView()
    : m_image_view(nullptr),
      m_num_faces(1)
{
}

ImageView::ImageView(VkImage image)
    : m_image(image),
      m_num_faces(1)
{
}

ImageView::~ImageView()
{
    AssertThrowMsg(m_image_view == nullptr, "image view should have been destroyed");
}

Result ImageView::Create(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    UInt mipmap_layer,
    UInt num_mipmaps,
    UInt face_layer,
    UInt num_faces
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

    HYPERION_VK_CHECK_MSG(
        vkCreateImageView(device->GetDevice(), &view_info, nullptr, &m_image_view),
        "Failed to create image view"
    );

    HYPERION_RETURN_OK;
}

Result ImageView::Create(
    Device *device,
    const Image *image,
    UInt mipmap_layer,
    UInt num_mipmaps,
    UInt face_layer,
    UInt num_faces
)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    return Create(
        device,
        image->GetGPUImage()->image,
        image->GetImageFormat(),
        ToVkImageAspect(image->GetTextureFormat()),
        ToVkImageViewType(image->GetType()),
        mipmap_layer,
        num_mipmaps,
        face_layer,
        num_faces
    );
}

Result ImageView::Create(Device *device, const Image *image)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    return Create(
        device,
        image->GetGPUImage()->image,
        image->GetImageFormat(),
        ToVkImageAspect(image->GetTextureFormat()),
        ToVkImageViewType(image->GetType()),
        0,
        image->NumMipmaps(),
        0,
        image->NumFaces()
    );
}

Result ImageView::Destroy(Device *device)
{
    vkDestroyImageView(device->GetDevice(), m_image_view, nullptr);
    m_image_view = nullptr;

    HYPERION_RETURN_OK;
}

VkImageAspectFlags ImageView::ToVkImageAspect(Image::InternalFormat internal_format)
{
    return Image::IsDepthFormat(internal_format)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageViewType ImageView::ToVkImageViewType(Image::Type type)
{
    switch (type) {
    case Image::Type::TEXTURE_TYPE_2D:      return VK_IMAGE_VIEW_TYPE_2D;
    case Image::Type::TEXTURE_TYPE_3D:      return VK_IMAGE_VIEW_TYPE_3D;
    case Image::Type::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    AssertThrowMsg(false, "Unhandled texture type case %d", int(type));
}

} // namespace renderer
} // namespace hyperion