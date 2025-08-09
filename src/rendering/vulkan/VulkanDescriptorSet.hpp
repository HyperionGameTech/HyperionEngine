/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/Defines.hpp>

#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanDescriptorSetLayoutWrapper;
using VulkanDescriptorSetLayoutWrapperRef = RenderObjectHandle_Strong<VulkanDescriptorSetLayoutWrapper>;

struct VulkanDescriptorElementInfo
{
    uint32 binding;
    uint32 index;
    VkDescriptorType descriptorType;

    union
    {
        VkDescriptorBufferInfo bufferInfo;
        VkDescriptorImageInfo imageInfo;
        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureInfo;
    };
};

class VulkanDescriptorSet final : public DescriptorSetBase
{
    using ElementCache = HashMap<Name, Array<VulkanDescriptorElementInfo>>;

public:
    VulkanDescriptorSet(const DescriptorSetLayout& layout);
    ~VulkanDescriptorSet();

    HYP_FORCE_INLINE VkDescriptorSet GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VulkanDescriptorSetLayoutWrapper* GetVulkanLayoutWrapper() const
    {
        return m_vkLayoutWrapper.Get();
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

    virtual void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    virtual void Update(bool force = false) override;

    virtual DescriptorSetRef Clone() const override;

#ifdef HYP_DEBUG_MODE
    virtual void SetDebugName(Name name) override;
#endif

protected:
    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, const ArrayMap<WeakName, uint32>& offsets, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, const ArrayMap<WeakName, uint32>& offsets, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, const ArrayMap<WeakName, uint32>& offsets, uint32 bindIndex) const override;

    VkDescriptorSet m_handle;
    ElementCache m_cachedElements;
    VulkanDescriptorSetLayoutWrapperRef m_vkLayoutWrapper;
    Array<VulkanDescriptorElementInfo> m_vkDescriptorElementInfos;
};

class VulkanDescriptorTable final : public DescriptorTableBase
{
public:
    VulkanDescriptorTable(const DescriptorTableDeclaration* decl);
    virtual ~VulkanDescriptorTable() override = default;
};

} // namespace hyperion
