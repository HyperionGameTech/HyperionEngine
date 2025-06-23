/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_ASYNC_COMPUTE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_ASYNC_COMPUTE_HPP

#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanAsyncCompute final : public AsyncComputeBase
{
public:
    HYP_API VulkanAsyncCompute();
    HYP_API virtual ~VulkanAsyncCompute() override;

    virtual bool IsSupported() const override
    {
        return m_is_supported;
    }

    HYP_API RendererResult Create();
    HYP_API RendererResult Submit(VulkanFrame* frame);

    HYP_API RendererResult PrepareForFrame(VulkanFrame* frame);
    HYP_API RendererResult WaitForFence(VulkanFrame* frame);

private:
    FixedArray<VulkanCommandBufferRef, max_frames_in_flight> m_command_buffers;
    FixedArray<VulkanFenceRef, max_frames_in_flight> m_fences;
    bool m_is_supported;
    bool m_is_fallback;
};

} // namespace hyperion

#endif