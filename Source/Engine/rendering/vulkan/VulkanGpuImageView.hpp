/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuImageView.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanGpuImage;

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanGpuImageView final : public GpuImageViewBase
{
    HYP_OBJECT_BODY(VulkanGpuImageView);

public:
    explicit VulkanGpuImageView(const VulkanGpuImageRef& image);

    VulkanGpuImageView(
        const VulkanGpuImageRef& image,
        const ImageSubResource& subResource);

    VulkanGpuImageView(
        const VulkanGpuImageRef& image,
        const ImageSubResource& subResource,
        VkImageViewType viewType);

    ~VulkanGpuImageView() override;

    HYP_FORCE_INLINE VkImageView GetVulkanHandle() const
    {
        return m_handle;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    VkImageView m_handle;
    VkImageViewType m_viewType;
};

} // namespace Hyperion
