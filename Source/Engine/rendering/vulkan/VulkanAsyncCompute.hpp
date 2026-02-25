/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

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

extern Pool* g_vulkanPool;

class VulkanAsyncCompute final : public AsyncComputeBase
{
    friend class VulkanRenderInterface;

public:
    HYP_DEF_POOL_NEW_DELETE(g_vulkanPool);

    VulkanAsyncCompute();
    ~VulkanAsyncCompute() override;

    bool IsSupported() const override
    {
        return m_isSupported;
    }

    bool CheckStatus() override;
    
    void Create() override;

    HYP_FORCE_INLINE VulkanCommandBuffer* GetCommandBuffer() const
    {
        return m_commandBuffer;
    }

    HYP_FORCE_INLINE VulkanFence* GetFence() const
    {
        return m_fence;
    }

private:
    void Submit();

    VulkanCommandBuffer* m_commandBuffer;
    VulkanFence* m_fence;
    VulkanDeviceQueue* m_deviceQueue;

    bool m_isSupported : 1;
    bool m_isSubmitted : 1;
};

} // namespace Hyperion
