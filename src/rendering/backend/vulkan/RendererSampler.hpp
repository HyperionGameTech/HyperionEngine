/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_RENDERER_VULKAN_SAMPLER_HPP
#define HYPERION_RENDERER_BACKEND_RENDERER_VULKAN_SAMPLER_HPP

#include <rendering/backend/RendererSampler.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanSampler final : public SamplerBase
{
public:
    HYP_API VulkanSampler(
        TextureFilterMode min_filter_mode = TFM_NEAREST,
        TextureFilterMode mag_filter_mode = TFM_NEAREST,
        TextureWrapMode wrap_mode = TWM_CLAMP_TO_EDGE);

    HYP_API virtual ~VulkanSampler() override;

    HYP_FORCE_INLINE VkSampler GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    VkSampler m_handle;
};

} // namespace hyperion

#endif