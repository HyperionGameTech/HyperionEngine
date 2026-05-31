/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/DescriptorSet.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Name/Name.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Containers/ArrayMap.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Defines.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <Vulkan/vulkan.h>

namespace Hyperion {

class VulkanDescriptorSetLayoutWrapper;

extern Pool* g_vulkanPool;

struct VulkanCachedDescriptor
{
    uint32 binding;
    uint32 index;
    VkDescriptorType descriptorType;

    union
    {
        VkDescriptorBufferInfo bufferInfo;
        VkDescriptorImageInfo imageInfo;
        VkAccelerationStructureKHR accelerationStructure;
    };

    bool operator==(const VulkanCachedDescriptor& other) const
    {
        static_assert(sizeof(VkDescriptorBufferInfo) == sizeof(VkDescriptorImageInfo));

        if (binding != other.binding
            || index != other.index
            || descriptorType != other.descriptorType)
        {
            return false;
        }

        if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            return accelerationStructure == other.accelerationStructure;
        }

        // For buffer and image info, we can do a memory comparison
        return Memory::Compare(&bufferInfo, &other.bufferInfo, sizeof(VkDescriptorBufferInfo)) == 0;
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

    using ElementCache = TMap<Name, Array<VulkanCachedDescriptor, VulkanAllocator>, VulkanAllocator>;

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

#if HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

protected:
    VkDescriptorSet m_handle;
    VkDescriptorPool m_vkDescriptorPool;
    VkDescriptorSetLayout m_vkDescriptorSetLayout;
    Array<VulkanCachedDescriptor, VulkanAllocator> m_pendingDescriptors;
    ElementCache m_cachedElements;
};

HYP_CLASS(NoScriptBindings)
class VulkanDescriptorTable final : public DescriptorTableBase
{
    HYP_OBJECT_BODY(VulkanDescriptorTable);

public:
    explicit VulkanDescriptorTable(const ShaderInputGroup* decl)
        : DescriptorTableBase(decl)
    {
    }

    ~VulkanDescriptorTable() override = default;
};

} // namespace Hyperion
