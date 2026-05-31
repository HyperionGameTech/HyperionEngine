/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/CommandBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/vulkan/VulkanDevice.hpp>
#include <Rendering/vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/vulkan/VulkanFence.hpp>
#include <Rendering/vulkan/VulkanSemaphore.hpp>

#include <Core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Core/Types.hpp>

namespace Hyperion {

class VulkanRenderPass;
struct VulkanDeviceQueue;
class VulkanPipelineBase;

constexpr uint32 MaxVulkanDynamicOffsets = 16;

extern Pool* g_vulkanPool;

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
    friend class VulkanGraphicsPipeline;
    friend class VulkanComputePipeline;
    friend class VulkanRayTracingPipeline;

    VulkanCommandBuffer();

    VulkanCommandBuffer(const VulkanCommandBuffer& other) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer& other) = delete;

    VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;

    ~VulkanCommandBuffer() override;

    HYP_FORCE_INLINE VkCommandBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkCommandPool GetVulkanCommandPool() const
    {
        return m_commandPool;
    }

    HYP_FORCE_INLINE bool IsInRenderPass() const
    {
        return m_renderPass != nullptr;
    }

    bool IsCreated() const override;

    RendererResult Create() override;
    RendererResult Create(VkCommandPool commandPool);

    bool IsRecording() const override
    {
        return m_isRecording;
    }

    void Begin() override;
    void End() override;

    void Reset();

    RendererResult Submit(
        VulkanDeviceQueue* queue,
        VulkanFence* fence,
        Span<VulkanSemaphore*> waitSemaphores,
        Span<VulkanSemaphore*> signalSemaphores);

    RendererResult Submit(
        VulkanDeviceQueue* queue,
        VulkanFence* fence,
        Span<VulkanSemaphore*> waitSemaphores,
        Span<VulkanSemaphore*> signalSemaphores,
        const uint64* waitValues,
        const uint64* signalValues);

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

#if HYP_DEBUG_MODE
    void SetDebugName(Name name);
#endif

    void ResetBoundDescriptorSets()
    {
        m_boundDescriptorSets.Clear();
    }

private:
    VkCommandBuffer m_handle;
    VkCommandPool m_commandPool;

    Array<VulkanCachedDescriptorSetBinding> m_boundDescriptorSets;

    bool m_isRecording;

public:
    VulkanRenderPass* m_renderPass;
    VulkanGraphicsPipeline* m_boundGraphicsPipeline;
    VulkanComputePipeline* m_boundComputePipeline;
    VulkanRayTracingPipeline* m_boundRayTracingPipeline;
};

} // namespace Hyperion
