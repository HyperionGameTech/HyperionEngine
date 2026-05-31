/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/Vulkan/VulkanDescriptorSet.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Vulkan/vulkan.h>

namespace Hyperion {

template <class PipelineType>
Array<VkDescriptorSetLayout, VulkanAllocator> GetVkDescriptorSetLayouts(const PipelineType& pipeline);

extern Pool* g_vulkanPool;

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

#if HYP_DEBUG_MODE
    void SetDebugName(Name name);
    void SetDebugNameLayout(Name name);
#endif

protected:
    VkPipeline m_handle;
    VkPipelineLayout m_layout;
};

} // namespace Hyperion
