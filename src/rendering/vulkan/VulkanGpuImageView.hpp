/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderGpuImageView.hpp>
#undef INCLUDE_FROM_RHI
#else
#undef INCLUDE_FROM_RHI_BASE
#endif

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanGpuImage;

HYP_CLASS(NoScriptBindings)
class VulkanGpuImageView final : public GpuImageViewBase
{
    HYP_OBJECT_BODY(VulkanGpuImageView);

public:
    VulkanGpuImageView(
        const VulkanGpuImageRef& image);

    VulkanGpuImageView(
        const VulkanGpuImageRef& image,
        uint32 mipIndex,
        uint32 numMips,
        uint32 layerIndex,
        uint32 numLayers);

    virtual ~VulkanGpuImageView() override;

    HYP_FORCE_INLINE VkImageView GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    VkImageView m_handle;
};

} // namespace hyperion
