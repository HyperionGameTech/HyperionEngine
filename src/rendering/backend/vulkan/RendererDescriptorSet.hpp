/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET_HPP
#define HYPERION_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET_HPP

#include <core/Name.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/Defines.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanDescriptorSetLayoutWrapper;
using VulkanDescriptorSetLayoutWrapperRef = RenderObjectHandle_Strong<VulkanDescriptorSetLayoutWrapper>;

struct VulkanDescriptorElementInfo
{
    uint32 binding;
    uint32 index;
    VkDescriptorType descriptor_type;

    union
    {
        VkDescriptorBufferInfo buffer_info;
        VkDescriptorImageInfo image_info;
        VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info;
    };
};

class VulkanDescriptorSet final : public DescriptorSetBase
{
    using ElementCache = HashMap<Name, Array<VulkanDescriptorElementInfo>, HashTable_DynamicNodeAllocator<KeyValuePair<Name, Array<VulkanDescriptorElementInfo>>>>;

public:
    HYP_API VulkanDescriptorSet(const DescriptorSetLayout& layout);
    HYP_API ~VulkanDescriptorSet();

    HYP_FORCE_INLINE VkDescriptorSet GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VulkanDescriptorSetLayoutWrapper* GetVulkanLayoutWrapper() const
    {
        return m_vk_layout_wrapper.Get();
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void UpdateDirtyState(bool* out_is_dirty = nullptr) override;
    HYP_API virtual void Update(bool force = false) override;

    HYP_API virtual DescriptorSetRef Clone() const override;

protected:
    virtual void Bind(const CommandBufferBase* command_buffer, const GraphicsPipelineBase* pipeline, uint32 bind_index) const override;
    virtual void Bind(const CommandBufferBase* command_buffer, const GraphicsPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index) const override;
    virtual void Bind(const CommandBufferBase* command_buffer, const ComputePipelineBase* pipeline, uint32 bind_index) const override;
    virtual void Bind(const CommandBufferBase* command_buffer, const ComputePipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index) const override;
    virtual void Bind(const CommandBufferBase* command_buffer, const RaytracingPipelineBase* pipeline, uint32 bind_index) const override;
    virtual void Bind(const CommandBufferBase* command_buffer, const RaytracingPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index) const override;

    VkDescriptorSet m_handle;
    ElementCache m_cached_elements;
    VulkanDescriptorSetLayoutWrapperRef m_vk_layout_wrapper;
    Array<VulkanDescriptorElementInfo> m_vk_descriptor_element_infos;
};

class VulkanDescriptorTable : public DescriptorTableBase
{
public:
    HYP_API VulkanDescriptorTable(const DescriptorTableDeclaration* decl);
    HYP_API virtual ~VulkanDescriptorTable() override = default;
};

} // namespace hyperion

#endif