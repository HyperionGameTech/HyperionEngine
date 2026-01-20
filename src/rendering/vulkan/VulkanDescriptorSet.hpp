/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/DescriptorSet.hpp>
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

namespace Hyperion {

class VulkanDescriptorSetLayoutWrapper;

struct VulkanCachedDescriptor
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

    HYP_FORCE_INLINE bool operator==(const VulkanCachedDescriptor& other) const
    {
        static_assert(sizeof(VkDescriptorBufferInfo) == sizeof(VkDescriptorImageInfo));

        return binding == other.binding
            && index == other.index
            && descriptorType == other.descriptorType
            && (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
                    ? (accelerationStructureInfo.accelerationStructureCount == other.accelerationStructureInfo.accelerationStructureCount
                        && Memory::MemCmpSafe(accelerationStructureInfo.pAccelerationStructures, other.accelerationStructureInfo.pAccelerationStructures, sizeof(const VkAccelerationStructureKHR*) * accelerationStructureInfo.accelerationStructureCount) == 0)
                    : Memory::MemCmpSafe(&bufferInfo, &other.bufferInfo, sizeof(VkDescriptorBufferInfo)) == 0);
    }

    HYP_FORCE_INLINE bool operator!=(const VulkanCachedDescriptor& other) const
    {
        return !(*this == other);
    }
};

HYP_CLASS(NoScriptBindings)
class VulkanDescriptorSet final : public DescriptorSetBase
{
    HYP_OBJECT_BODY(VulkanDescriptorSet);

    using ElementCache = HashMap<Name, Array<VulkanCachedDescriptor>>;

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

    bool IsCreated() const override;

    RendererResult Create() override;

    void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    void Update(bool force = false) override;

    void Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    void Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, uint32 bindIndex) const override;
    void Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    void Bind(VulkanCommandBuffer* commandBuffer, const VulkanRayTracingPipeline* pipeline, uint32 bindIndex) const override;
    void Bind(VulkanCommandBuffer* commandBuffer, const VulkanRayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const override;

    VulkanDescriptorSetRef Clone() const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

protected:
    VkDescriptorSet m_handle;
    VkDescriptorPool m_vkDescriptorPool;
    VkDescriptorSetLayout m_vkDescriptorSetLayout;
    Array<VulkanCachedDescriptor> m_pendingDescriptors;
    ElementCache m_cachedElements;
};

HYP_CLASS(NoScriptBindings)
class VulkanDescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(VulkanDescriptorTable);

public:
    explicit VulkanDescriptorTable(const DescriptorTableDeclaration* decl)
        : DescriptorTableBase(decl)
    {
    }
    
    ~VulkanDescriptorTable() override = default;
};

} // namespace Hyperion
