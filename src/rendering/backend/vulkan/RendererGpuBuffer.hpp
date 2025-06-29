/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP

#include <rendering/backend/RendererGpuBuffer.hpp>

#include <system/vma/VmaUsage.hpp>

namespace hyperion {
class VulkanGpuBuffer final : public GpuBufferBase
{
public:
    HYP_API VulkanGpuBuffer(GpuBufferType type, SizeType size, SizeType alignment = 0);
    HYP_API virtual ~VulkanGpuBuffer() override;

    HYP_FORCE_INLINE VkBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_API virtual bool IsCreated() const override;
    HYP_API virtual bool IsCpuAccessible() const override;

    HYP_API virtual void InsertBarrier(CommandBufferBase* command_buffer, ResourceState new_state) const override;
    HYP_API virtual void InsertBarrier(CommandBufferBase* command_buffer, ResourceState new_state, ShaderModuleType shader_type) const override;

    HYP_API void InsertBarrier(VulkanCommandBuffer* command_buffer, ResourceState new_state) const;
    HYP_API void InsertBarrier(VulkanCommandBuffer* command_buffer, ResourceState new_state, ShaderModuleType shader_type) const;

    HYP_API virtual void CopyFrom(
        CommandBufferBase* command_buffer,
        const GpuBufferBase* src_buffer,
        SizeType count) override;

    HYP_API RendererResult CheckCanAllocate(SizeType size) const;

    HYP_API uint64 GetBufferDeviceAddress() const;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual RendererResult EnsureCapacity(
        SizeType minimum_size,
        bool* out_size_changed = nullptr) override;

    HYP_API virtual RendererResult EnsureCapacity(
        SizeType minimum_size,
        SizeType alignment,
        bool* out_size_changed = nullptr) override;

    HYP_API virtual void Memset(SizeType count, ubyte value) override;

    HYP_API virtual void Copy(SizeType count, const void* ptr) override;
    HYP_API virtual void Copy(SizeType offset, SizeType count, const void* ptr) override;

    HYP_API virtual void Read(SizeType count, void* out_ptr) const override;
    HYP_API virtual void Read(SizeType offset, SizeType count, void* out_ptr) const override;

    HYP_API virtual void Map() const override;
    HYP_API virtual void Unmap() const override;

private:
    RendererResult CheckCanAllocate(
        const VkBufferCreateInfo& buffer_create_info,
        const VmaAllocationCreateInfo& allocation_create_info,
        SizeType size) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo() const;
    VkBufferCreateInfo GetBufferCreateInfo() const;

    VkBuffer m_handle = VK_NULL_HANDLE;

    VkBufferUsageFlags m_vk_buffer_usage_flags = 0;
    VmaMemoryUsage m_vma_usage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationCreateFlags m_vma_allocation_create_flags = 0;
    VmaAllocation m_vma_allocation = VK_NULL_HANDLE;

    mutable void* m_mapping = nullptr;
};

} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
