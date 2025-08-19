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

HYP_CLASS(NoScriptBindings)
class VulkanDescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(VulkanDescriptorSet);

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

    virtual void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    virtual void Update(bool force = false) override;

    virtual DescriptorSetRef Clone() const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

protected:
    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    VkDescriptorSet m_handle;
    VkDescriptorPool m_vkDescriptorPool;
    ElementCache m_cachedElements;
    RC<VulkanDescriptorSetLayoutWrapper> m_vkLayoutWrapper;
    Array<VulkanDescriptorElementInfo> m_vkDescriptorElementInfos;
};

HYP_CLASS()
class VulkanDescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(VulkanDescriptorTable);

public:
    VulkanDescriptorTable(const DescriptorTableDeclaration* decl);
    virtual ~VulkanDescriptorTable() override = default;
};

} // namespace hyperion
