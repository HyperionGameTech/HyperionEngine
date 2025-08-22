/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderGpuBuffer.hpp>

#include <system/vma/VmaUsage.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class VulkanGpuBuffer final : public GpuBufferBase
{
    HYP_OBJECT_BODY(VulkanGpuBuffer);

public:
    VulkanGpuBuffer(GpuBufferType type, SizeType size, SizeType alignment = 0);
    virtual ~VulkanGpuBuffer() override;

    HYP_FORCE_INLINE VkBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    virtual bool IsCreated() const override;
    virtual bool IsCpuAccessible() const override;

    virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState) const override;
    virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const override;

    void InsertBarrier(VulkanCommandBuffer* commandBuffer, ResourceState newState) const;
    void InsertBarrier(VulkanCommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const;

    virtual void CopyFrom(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer,
        uint32 count) override;

    virtual void CopyFrom(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer,
        uint32 srcOffset, uint32 dstOffset,
        uint32 count) override;

    RendererResult CheckCanAllocate(SizeType size) const;

    uint64 GetBufferDeviceAddress() const;

    virtual RendererResult Create() override;

    virtual RendererResult EnsureCapacity(
        SizeType minimumSize,
        bool* outSizeChanged = nullptr) override;

    virtual RendererResult EnsureCapacity(
        SizeType minimumSize,
        SizeType alignment,
        bool* outSizeChanged = nullptr) override;

    virtual void Memset(SizeType count, ubyte value) override;

    virtual void Copy(SizeType count, const void* ptr) override;
    virtual void Copy(SizeType offset, SizeType count, const void* ptr) override;

    virtual void Read(SizeType count, void* outPtr) const override;
    virtual void Read(SizeType offset, SizeType count, void* outPtr) const override;

    virtual void Map() const override;
    virtual void Unmap() const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult CheckCanAllocate(
        const VkBufferCreateInfo& bufferCreateInfo,
        const VmaAllocationCreateInfo& allocationCreateInfo,
        SizeType size) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo() const;
    VkBufferCreateInfo GetBufferCreateInfo() const;

    VkBuffer m_handle = VK_NULL_HANDLE;

    VkBufferUsageFlags m_vkBufferUsageFlags = 0;
    VmaMemoryUsage m_vmaUsage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationCreateFlags m_vmaAllocationCreateFlags = 0;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;

    mutable void* m_mapping = nullptr;
};

} // namespace hyperion
