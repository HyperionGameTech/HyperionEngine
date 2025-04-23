/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP

#include <rendering/backend/RendererImageView.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class VulkanImage;

class VulkanImageView final : public ImageViewBase
{
public:
    static constexpr PlatformType platform = Platform::VULKAN;
    
    HYP_API VulkanImageView(
        const VulkanImageRef &image
    );

    HYP_API VulkanImageView(
        const VulkanImageRef &image,
        uint32 mip_index,
        uint32 num_mips,
        uint32 face_index,
        uint32 num_faces
    );

    HYP_API virtual ~VulkanImageView() override;
    
    HYP_FORCE_INLINE VkImageView GetVulkanHandle() const
        { return m_handle; }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    VkImageView m_handle;
};

} // namespace renderer
} // namespace hyperion

#endif