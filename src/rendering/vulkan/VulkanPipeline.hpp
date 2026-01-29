/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

template <class PipelineType>
Array<VkDescriptorSetLayout> GetVkDescriptorSetLayouts(const PipelineType& pipeline);

class VulkanPipelineBase
{
public:
    VulkanPipelineBase();
    ~VulkanPipelineBase();

    HYP_FORCE_INLINE VkPipeline GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkPipelineLayout GetVulkanPipelineLayout() const
    {
        return m_layout;
    }

    bool IsCreated() const;

    void SetPushConstants(const void* data, SizeType size);

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name);
#endif

protected:
    VkPipeline m_handle;
    VkPipelineLayout m_layout;

    PushConstantData m_pushConstants;
};

} // namespace Hyperion
