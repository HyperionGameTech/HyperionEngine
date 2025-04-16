/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_RENDERER_VULKAN_SAMPLER_HPP
#define HYPERION_RENDERER_BACKEND_RENDERER_VULKAN_SAMPLER_HPP

#include <rendering/backend/RendererSampler.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class VulkanSampler final : public SamplerBase
{
public:
    HYP_API VulkanSampler(
        FilterMode min_filter_mode = FilterMode::TEXTURE_FILTER_NEAREST,
        FilterMode mag_filter_mode = FilterMode::TEXTURE_FILTER_NEAREST,
        WrapMode wrap_mode = WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    );

    HYP_API virtual ~VulkanSampler() override;
    
    HYP_FORCE_INLINE VkSampler GetVulkanHandle() const
        { return m_handle; }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

private:
    VkSampler   m_handle;
};

} // namespace renderer
} // namespace hyperion

#endif