/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/AsyncCompute.hpp>
#undef INCLUDE_FROM_RHI
#else
#undef INCLUDE_FROM_RHI_BASE
#endif

#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanAsyncCompute final : public AsyncComputeBase
{
public:
    VulkanAsyncCompute();
    virtual ~VulkanAsyncCompute() override;

    virtual bool IsSupported() const override
    {
        return m_isSupported;
    }

    RendererResult Create();
    RendererResult Submit(VulkanFrame* frame);

    RendererResult PrepareForFrame(VulkanFrame* frame);
    RendererResult WaitForFence(VulkanFrame* frame);

private:
    FixedArray<VulkanCommandBufferRef, NumFramesInFlight> m_commandBuffers;
    FixedArray<VulkanFenceRef, NumFramesInFlight> m_fences;
    bool m_isSupported;
    bool m_isFallback;
};

} // namespace hyperion
