/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/AsyncCompute.hpp>
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
