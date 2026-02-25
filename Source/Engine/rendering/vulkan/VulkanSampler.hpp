/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Sampler.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <vulkan/vulkan.h>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanSampler final : public SamplerBase
{
    HYP_OBJECT_BODY(VulkanSampler);

public:
    VulkanSampler(
        TextureFilterMode minFilterMode = TFM_NEAREST,
        TextureFilterMode magFilterMode = TFM_NEAREST,
        TextureWrapMode wrapMode = TWM_CLAMP_TO_EDGE);

    ~VulkanSampler() override;

    HYP_FORCE_INLINE VkSampler GetVulkanHandle() const
    {
        return m_handle;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    VkSampler m_handle;
};

} // namespace Hyperion
