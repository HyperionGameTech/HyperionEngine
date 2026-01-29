/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/CommandBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>

#include <core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <core/Types.hpp>

namespace Hyperion {

class VulkanRenderPass;
struct VulkanDeviceQueue;
class VulkanPipelineBase;

constexpr uint32 MaxVulkanDynamicOffsets = 16;

struct VulkanCachedDescriptorSetBinding
{
    VkDescriptorSet descriptorSet;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    
    uint32 numDynamicOffsets;
    uint32 dynamicOffsets[MaxVulkanDynamicOffsets];

    HYP_FORCE_INLINE bool operator==(const VulkanCachedDescriptorSetBinding& other) const
    {
        return descriptorSet == other.descriptorSet
            && pipeline == other.pipeline
            && pipelineLayout == other.pipelineLayout
            && numDynamicOffsets == other.numDynamicOffsets
            && Memory::Compare(dynamicOffsets, other.dynamicOffsets, numDynamicOffsets * sizeof(uint32)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const VulkanCachedDescriptorSetBinding& other) const
    {
        return descriptorSet != other.descriptorSet
            || pipeline != other.pipeline
            || pipelineLayout != other.pipelineLayout
            || numDynamicOffsets != other.numDynamicOffsets
            || Memory::Compare(dynamicOffsets, other.dynamicOffsets, numDynamicOffsets * sizeof(uint32)) != 0;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode((void*)descriptorSet)
            .Combine((void*)pipeline)
            .Combine((void*)pipelineLayout)
            .Combine(numDynamicOffsets)
            .Combine(HashCode::GetHashCode((const ubyte*)dynamicOffsets, (const ubyte*)(dynamicOffsets + numDynamicOffsets)));
    }
};

HYP_CLASS(NoScriptBindings)
class VulkanCommandBuffer final : public CommandBufferBase
{
    HYP_OBJECT_BODY(VulkanCommandBuffer);

public:
    friend class VulkanFramebuffer;
    friend class VulkanDescriptorSet;

    explicit VulkanCommandBuffer(VkCommandBufferLevel type);
    ~VulkanCommandBuffer() override;

    HYP_FORCE_INLINE VkCommandBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkCommandPool GetVulkanCommandPool() const
    {
        return m_commandPool;
    }

    HYP_FORCE_INLINE VkCommandBufferLevel GetType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE bool IsInRenderPass() const
    {
        return m_isInRenderPass;
    }

    bool IsCreated() const override;

    RendererResult Create() override;
    RendererResult Create(VkCommandPool commandPool);

    RendererResult Begin(const VulkanRenderPass* renderPass = nullptr);
    RendererResult End();
    RendererResult Reset();
    RendererResult SubmitPrimary(
        VulkanDeviceQueue* queue,
        VulkanFence* fence,
        Span<VulkanSemaphore*> waitSemaphores,
        Span<VulkanSemaphore*> signalSemaphores);

    RendererResult SubmitSecondary(VulkanCommandBuffer* primary);

    void BindVertexBuffer(const VulkanGpuBuffer* buffer) override;
    void BindIndexBuffer(const VulkanGpuBuffer* buffer, GpuElemType elemType = GET_UNSIGNED_INT) override;

    void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const override;

    void DrawIndexedIndirect(
        const VulkanGpuBuffer* buffer,
        uint32 bufferOffset) const override;

    void DebugMarkerBegin(const char* markerName) const;
    void DebugMarkerEnd() const;

    template <class LambdaFunction>
    RendererResult Record(const VulkanRenderPass* renderPass, LambdaFunction&& fn)
    {
        CheckResultOrReturn(Begin(renderPass));
        CheckResultOrReturn(fn(this));
        CheckResultOrReturn(End());

        return {};
    }

    void ResetBoundDescriptorSets()
    {
        m_boundDescriptorSets.Clear();
    }

private:
    VkCommandBufferLevel m_type;
    VkCommandBuffer m_handle;
    VkCommandPool m_commandPool;

    Array<VulkanCachedDescriptorSetBinding> m_boundDescriptorSets;

    bool m_isInRenderPass : 1;
};

} // namespace Hyperion
