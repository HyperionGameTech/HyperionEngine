/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/RenderBackend.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

VulkanFeatures::VulkanFeatures()
    : m_physicalDevice(nullptr),
      m_properties({}),
      m_features({})
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

        HYP_GFX_ASSERT(m_features.samplerAnisotropy);

        auto AddToFeaturesChain = [this](auto nextFeature)
        {
            using T = decltype(nextFeature);

            VkBaseOutStructure* chainTop = m_featuresChain.Empty()
                ? nullptr
                : m_featuresChain.Back().Get();

            m_featuresChain.PushBack(MakeUnique<VkBaseOutStructure>(new T(nextFeature)));

            if (chainTop != nullptr)
            {
                chainTop->pNext = m_featuresChain.Back().Get();
            }

            chainTop = m_featuresChain.Back().Get();
        };

        // features

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        m_bufferDeviceAddressFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = VK_NULL_HANDLE
        };

        m_raytracingPipelineFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &m_bufferDeviceAddressFeatures
        };

        m_accelerationStructureFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &m_raytracingPipelineFeatures
        };

        m_rayQueryFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext = &m_accelerationStructureFeatures
        };

        m_multiviewFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
            .pNext = &m_rayQueryFeatures
        };
#else
        m_multiviewFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
            .pNext = VK_NULL_HANDLE
        };
#endif

        m_indexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
            .pNext = &m_multiviewFeatures
        };

        m_features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &m_indexingFeatures
        };

        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &m_features2);

        // properties

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        m_raytracingPipelineProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
            .pNext = VK_NULL_HANDLE
        };

        m_accelerationStructureProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &m_raytracingPipelineProperties
        };

        m_samplerMinmaxProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
            .pNext = &m_accelerationStructureProperties
        };
#else
        m_samplerMinmaxProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,
            .pNext = VK_NULL_HANDLE
        };
#endif

        m_indexingProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
            .pNext = &m_samplerMinmaxProperties
        };

        m_properties2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &m_indexingProperties
        };

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
#ifdef HYP_DEBUG_MODE
        mvkConfig[i].debugMode = true;
#endif
    }

    g_vulkanDynamicFunctions->vkSetMoltenVKConfigurationMVK(VK_NULL_HANDLE, mvkConfig, &sz);

    delete[] mvkConfig;
#endif
}

} // namespace hyperion
