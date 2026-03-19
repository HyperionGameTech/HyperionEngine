/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanFeatures.hpp>

#include <rendering/RenderInterface.hpp>

namespace Hyperion {

VulkanFeatures::VulkanFeatures()
    : m_physicalDevice(nullptr),
      m_properties { },
      m_features { }
{
}

VulkanFeatures::VulkanFeatures(VkPhysicalDevice physicalDevice)
    : VulkanFeatures()
{
    SetPhysicalDevice(physicalDevice);
}

void VulkanFeatures::SetPhysicalDevice(VkPhysicalDevice physicalDevice)
{
    if ((m_physicalDevice = physicalDevice))
    {
        m_features.samplerAnisotropy = VK_TRUE;

        vkGetPhysicalDeviceProperties(physicalDevice, &m_properties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &m_features);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);

        Assert(m_features.samplerAnisotropy);
        
        m_features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
        };

        m_multiviewFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR
        };
        VulkanHelpers::ChainNext(m_features2, &m_multiviewFeatures);

        m_indexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT
        };
        VulkanHelpers::ChainNext(m_features2, &m_indexingFeatures);

        m_scalarBlockLayoutFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES
        };
        VulkanHelpers::ChainNext(m_features2, &m_scalarBlockLayoutFeatures);

#if defined(HYP_FEATURES_ENABLE_RAY_TRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        m_bufferDeviceAddressFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
        };
        VulkanHelpers::ChainNext(m_features2, &m_bufferDeviceAddressFeatures);

        m_rayTracingPipelineFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
        };
        VulkanHelpers::ChainNext(m_features2, &m_rayTracingPipelineFeatures);

        m_accelerationStructureFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
        };
        VulkanHelpers::ChainNext(m_features2, &m_accelerationStructureFeatures);

        m_rayQueryFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR
        };
        VulkanHelpers::ChainNext(m_features2, &m_rayQueryFeatures);
#endif

        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &m_features2);

        // properties
        m_properties2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
        };

        m_indexingProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT
        };
        VulkanHelpers::ChainNext(m_properties2, &m_indexingProperties);

        m_samplerMinmaxProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT
        };
        VulkanHelpers::ChainNext(m_properties2, &m_samplerMinmaxProperties);

#if defined(HYP_FEATURES_ENABLE_RAY_TRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        m_rayTracingPipelineProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };
        VulkanHelpers::ChainNext(m_properties2, &m_rayTracingPipelineProperties);

        m_accelerationStructureProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
        };
        VulkanHelpers::ChainNext(m_properties2, &m_accelerationStructureProperties);
#endif

        vkGetPhysicalDeviceProperties2(m_physicalDevice, &m_properties2);
    }
}

void VulkanFeatures::SetDeviceFeatures(VulkanDevice* device)
{
#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    MVKConfiguration* mvkConfig = nullptr;
    size_t sz = 1;
    g_vulkanDynamicFunctions->vkGetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvkConfig, &sz);

    mvkConfig = new MVKConfiguration[sz];

    for (size_t i = 0; i < sz; i++)
    {
#if HYP_DEBUG_MODE
        mvkConfig[i].debugMode = true;
#endif
    }

    g_vulkanDynamicFunctions->vkSetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvkConfig, &sz);

    delete[] mvkConfig;
#endif
}

} // namespace Hyperion
