/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_PIPELINE_HPP

#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

extern HYP_API VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout);

template <class PipelineType>
static inline Array<VkDescriptorSetLayout> GetPipelineVulkanDescriptorSetLayouts(const PipelineType& pipeline)
{
    AssertThrowMsg(pipeline.GetDescriptorTable().IsValid(), "Invalid DescriptorTable provided to Pipeline");

    Array<VkDescriptorSetLayout> used_layouts;

    for (const DescriptorSetRef& descriptor_set : pipeline.GetDescriptorTable()->GetSets()[0])
    {
        VulkanDescriptorSetRef vulkan_descriptor_set = VulkanDescriptorSetRef(descriptor_set);

        AssertThrow(vulkan_descriptor_set != nullptr);
        AssertThrow(vulkan_descriptor_set->GetVulkanLayoutWrapper() != nullptr);

        used_layouts.PushBack(GetVkDescriptorSetLayout(*vulkan_descriptor_set->GetVulkanLayoutWrapper()));
    }

    return used_layouts;
}

class VulkanPipelineBase
{
public:
    HYP_API VulkanPipelineBase();
    HYP_API virtual ~VulkanPipelineBase();

    HYP_FORCE_INLINE VkPipeline GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkPipelineLayout GetVulkanPipelineLayout() const
    {
        return m_layout;
    }

    HYP_API Array<VkDescriptorSetLayout> GetDescriptorSetLayouts() const;

    HYP_API RendererResult Destroy();

    HYP_API bool IsCreated() const;

    HYP_API void SetPushConstants(const void* data, SizeType size);

protected:
    VkPipeline m_handle;
    VkPipelineLayout m_layout;

    PushConstantData m_push_constants;
};

} // namespace renderer
} // namespace hyperion

#endif
