/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderDescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <core/Name.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/Defines.hpp>

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

    HYP_FORCE_INLINE VkDescriptorSetLayout GetVulkanLayout() const
    {
        return m_vkDescriptorSetLayout;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

    virtual void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    virtual void Update(bool force = false) override;

    virtual void Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, uint32 bindIndex) const override;
    virtual void Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    virtual void Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, uint32 bindIndex) const override;
    virtual void Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;
    virtual void Bind(VulkanCommandBuffer* commandBuffer, const VulkanRaytracingPipeline* pipeline, uint32 bindIndex) const override;
    virtual void Bind(VulkanCommandBuffer* commandBuffer, const VulkanRaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    virtual VulkanDescriptorSetRef Clone() const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

protected:
    VkDescriptorSet m_handle;
    VkDescriptorPool m_vkDescriptorPool;
    VkDescriptorSetLayout m_vkDescriptorSetLayout;
    Array<VulkanDescriptorElementInfo> m_vkDescriptorElementInfos;
    ElementCache m_cachedElements;
};

HYP_CLASS(NoScriptBindings)
class VulkanDescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(VulkanDescriptorTable);

public:
    VulkanDescriptorTable(const DescriptorTableDeclaration* decl);
    virtual ~VulkanDescriptorTable() override = default;
};

} // namespace hyperion
