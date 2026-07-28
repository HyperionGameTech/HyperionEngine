/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/Vulkan/VulkanMemoryAllocator.hpp>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanGpuBuffer final : public GpuBufferBase
{
    HYP_OBJECT_BODY(VulkanGpuBuffer);

public:
    VulkanGpuBuffer(GpuBufferType type, size_t size, size_t alignment = 0);

    VulkanGpuBuffer(const VulkanGpuBuffer& other) = delete;
    VulkanGpuBuffer& operator=(const VulkanGpuBuffer& other) = delete;

    VulkanGpuBuffer(VulkanGpuBuffer&& other) noexcept;
    VulkanGpuBuffer& operator=(VulkanGpuBuffer&& other) noexcept;

    ~VulkanGpuBuffer() override;

    HYP_FORCE_INLINE VkBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkBufferUsageFlags GetBufferUsageFlags() const
    {
        return m_vkBufferUsageFlags;
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

    using GpuBufferBase::Invalidate;
    void Invalidate(size_t offset, size_t count) override;

    /*! \brief For readback buffers, record a memory dependency that makes the transfer writes
     *  visible to host reads. No-op for any other buffer type. */
    void InsertHostReadBarrier(VulkanCommandBuffer* commandBuffer) const;

#ifdef HYP_RHI_DEBUG_NAMES
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

    VkBufferUsageFlags m_vkBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaMemoryUsage m_vmaUsage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationCreateFlags m_vmaAllocationCreateFlags = 0;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;

    mutable void* m_mapping = nullptr;
};

} // namespace Hyperion
