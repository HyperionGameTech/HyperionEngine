/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

HYP_CLASS(NoScriptBindings)
class VulkanGpuImageView final : public GpuImageViewBase
{
    HYP_OBJECT_BODY(VulkanGpuImageView);

public:
    explicit VulkanGpuImageView(const VulkanGpuImageRef& image);
    VulkanGpuImageView(const VulkanGpuImageRef& image, const ImageSubResource& subResource);

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
};

} // namespace Hyperion
