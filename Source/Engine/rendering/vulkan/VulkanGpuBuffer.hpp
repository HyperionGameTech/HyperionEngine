/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanMemoryAllocator.hpp>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanGpuBuffer final : public GpuBufferBase
{
    HYP_OBJECT_BODY(VulkanGpuBuffer);

public:
    VulkanGpuBuffer(GpuBufferType type, size_t size, size_t alignment = 0);
    ~VulkanGpuBuffer() override;

    HYP_FORCE_INLINE VkBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    bool IsCreated() const override;
    bool IsCpuAccessible() const override;

    void InsertBarrier(VulkanCommandBuffer* commandBuffer, ResourceState newState) const override;
    void InsertBarrier(VulkanCommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const override;

    void CopyFrom(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuBuffer* srcBuffer,
        uint32 count) override;

    void CopyFrom(
        VulkanCommandBuffer* commandBuffer,
        const VulkanGpuBuffer* srcBuffer,
        uint32 srcOffset, uint32 dstOffset,
        uint32 count) override;

    RendererResult CheckCanAllocate(size_t size) const;

    uint64 GetBufferDeviceAddress() const;

    RendererResult Create() override;

    RendererResult EnsureCapacity(
        size_t minimumSize,
        bool* outSizeChanged = nullptr) override;

    RendererResult EnsureCapacity(
        size_t minimumSize,
        size_t alignment,
        bool* outSizeChanged = nullptr) override;

    void Memset(size_t count, ubyte value) override;

    void Copy(size_t count, const void* ptr) override;
    void Copy(size_t offset, size_t count, const void* ptr) override;

    void Read(size_t count, void* outPtr) const override;
    void Read(size_t offset, size_t count, void* outPtr) const override;

    void* Map() const override;
    void Unmap() const override;

    void Flush(size_t offset, size_t count) override;

#if HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult CheckCanAllocate(
        const VkBufferCreateInfo& bufferCreateInfo,
        const VmaAllocationCreateInfo& allocationCreateInfo,
        size_t size) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo() const;
    VkBufferCreateInfo GetBufferCreateInfo() const;

    VkBuffer m_handle = VK_NULL_HANDLE;

    VkBufferUsageFlags m_vkBufferUsageFlags = 0;
    VmaMemoryUsage m_vmaUsage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationCreateFlags m_vmaAllocationCreateFlags = 0;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;

    mutable void* m_mapping = nullptr;
};

} // namespace Hyperion
