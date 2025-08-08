/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderGpuImageView.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanGpuImage;

class VulkanGpuImageView final : public GpuImageViewBase
{
public:
    VulkanGpuImageView(
        const VulkanGpuImageRef& image);

    VulkanGpuImageView(
        const VulkanGpuImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 faceIndex,
        uint32 numFaces);

    virtual ~VulkanGpuImageView() override;

    HYP_FORCE_INLINE VkImageView GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

private:
    VkImageView m_handle;
};

} // namespace hyperion
