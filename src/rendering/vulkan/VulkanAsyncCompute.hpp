/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/AsyncCompute.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class VulkanAsyncCompute final : public AsyncComputeBase
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    VulkanAsyncCompute();
    ~VulkanAsyncCompute() override;

    bool IsSupported() const override
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

} // namespace Hyperion
