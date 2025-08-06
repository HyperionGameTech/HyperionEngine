/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <rendering/vulkan/VulkanResult.hpp>
#include <rendering/vulkan/VulkanImage.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/containers/Array.hpp>

#include <core/Defines.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <array>

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK
#include <MoltenVK/vk_mvk_moltenvk.h>
#endif

namespace hyperion {

class VulkanFeatures
{
public:
    VulkanFeatures();
    VulkanFeatures(VkPhysicalDevice);

    VulkanFeatures(const VulkanFeatures& other) = delete;
    VulkanFeatures& operator=(const VulkanFeatures& other) = delete;
    ~VulkanFeatures() = default;

    void SetPhysicalDevice(VkPhysicalDevice);

    VkPhysicalDevice GetPhysicalDevice() const
    {
        return m_physicalDevice;
    }

    bool IsDiscreteGpu() const
    {
        return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    const char* GetDeviceName() const
    {
        return m_properties.deviceName;
    }

    const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const
    {
        return m_properties;
    }

    const VkPhysicalDeviceFeatures& GetPhysicalDeviceFeatures() const
    {
        return m_features;
    }

    const VkPhysicalDeviceFeatures2& GetPhysicalDeviceFeatures2() const
    {
        return m_features2;
    }

    const VkPhysicalDeviceDescriptorIndexingFeatures& GetPhysicalDeviceIndexingFeatures() const
    {
        return m_indexingFeatures;
    }

    const VkPhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProperties() const
    {
        return m_memoryProperties;
    }

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& GetRaytracingPipelineFeatures() const
    {
        return m_raytracingPipelineFeatures;
    }

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRaytracingPipelineProperties() const
    {
        return m_raytracingPipelineProperties;
    }

    const VkPhysicalDeviceBufferDeviceAddressFeatures& GetBufferDeviceAddressFeatures() const
    {
        return m_bufferDeviceAddressFeatures;
    }

    const VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT& GetSamplerMinMaxProperties() const
    {
        return m_samplerMinmaxProperties;
    }

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR& GetAccelerationStructureFeatures() const
    {
        return m_accelerationStructureFeatures;
    }

    const VkPhysicalDeviceAccelerationStructurePropertiesKHR& GetAccelerationStructureProperties() const
    {
        return m_accelerationStructureProperties;
    }

    struct DeviceRequirementsResult
    {
        enum
        {
            DEVICE_REQUIREMENTS_OK = 0,
            DEVICE_REQUIREMENTS_ERR = 1
        } result;

        const char* message;

        DeviceRequirementsResult(decltype(result) result, const char* message = "")
            : result(result),
              message(message)
        {
        }

        DeviceRequirementsResult(const DeviceRequirementsResult& other)
            : result(other.result),
              message(other.message)
        {
        }

        operator bool() const
        {
            return result == DEVICE_REQUIREMENTS_OK;
        }
    };

#define REQUIRES_VK_FEATURE_MSG(cond, feature)                                                                                                      \
    do                                                                                                                                              \
    {                                                                                                                                               \
        if (!(cond))                                                                                                                                \
        {                                                                                                                                           \
            return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "Feature constraint '" #feature "' not satisfied."); \
        }                                                                                                                                           \
    }                                                                                                                                               \
    while (0)

#define REQUIRES_VK_FEATURE(cond)                                                                                                                \
    do                                                                                                                                           \
    {                                                                                                                                            \
        if (!(cond))                                                                                                                             \
        {                                                                                                                                        \
            return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "Feature constraint '" #cond "' not satisfied."); \
        }                                                                                                                                        \
    }                                                                                                                                            \
    while (0)

    DeviceRequirementsResult SatisfiesMinimumRequirements()
    {
        REQUIRES_VK_FEATURE_MSG(m_features.fragmentStoresAndAtomics, "Image stores and atomics in fragment shaders");
        REQUIRES_VK_FEATURE_MSG(m_multiviewFeatures.multiview, "Multiview not supported");
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetSamplers >= 16);
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetUniformBuffers >= 16);

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
        REQUIRES_VK_FEATURE(m_indexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers >= 4096);
#else
        REQUIRES_VK_FEATURE(m_indexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers >= 16);
#endif

        return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_OK);
    }

#undef REQUIRES_VK_FEATURE

    bool SupportsBindlessTextures() const
    {
#ifndef HYP_FEATURES_BINDLESS_TEXTURES
        return false;
#else

        return m_indexingFeatures.descriptorBindingPartiallyBound
            && m_indexingFeatures.runtimeDescriptorArray
            && m_indexingProperties.maxPerStageDescriptorUpdateAfterBindSamplers >= 4096
            && m_indexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages >= 4096;
#endif
    }

    bool SupportsDynamicDescriptorIndexing() const
    {
        return m_features.shaderSampledImageArrayDynamicIndexing;
    }

    void SetDeviceFeatures(VulkanDevice* device);

    VulkanSwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR _surface) const
    {
        HYP_GFX_ASSERT(m_physicalDevice != VK_NULL_HANDLE, "No physical device set!");
        
        VulkanSwapchainSupportDetails details {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, _surface, &details.capabilities);

        uint32 numQueueFamilies = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &numQueueFamilies, nullptr);

        Array<VkQueueFamilyProperties> queueFamilyProperties;
        queueFamilyProperties.Resize(numQueueFamilies);

        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &numQueueFamilies, queueFamilyProperties.Data());

        uint32 numSurfaceFormats = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, _surface, &numSurfaceFormats, nullptr);

        Array<VkSurfaceFormatKHR> surfaceFormats;
        surfaceFormats.Resize(numSurfaceFormats);

        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, _surface, &numSurfaceFormats, surfaceFormats.Data());
        HYP_GFX_ASSERT(surfaceFormats.Any(), "No surface formats available!");

        uint32 numPresentModes = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, _surface, &numPresentModes, nullptr);

        Array<VkPresentModeKHR> presentModes;
        presentModes.Resize(numPresentModes);

        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, _surface, &numPresentModes, presentModes.Data());
        HYP_GFX_ASSERT(presentModes.Any(), "No present modes available!");

        details.queueFamilyProperties = queueFamilyProperties;
        details.formats = surfaceFormats;
        details.presentModes = presentModes;

        return details;
    }

    bool IsSupportedFormat(
        TextureFormat format,
        ImageSupport supportType) const
    {
        if (m_physicalDevice == nullptr)
        {
            return false;
        }

        const VkFormat vulkanFormat = ToVkFormat(format);

        VkFormatFeatureFlags featureFlags = 0;

        switch (supportType)
        {
        case IS_SRV:
            featureFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
            break;
        case IS_UAV:
            featureFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
            break;
        case IS_DEPTH:
            featureFlags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        default:
            HYP_UNREACHABLE();
        }

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, vulkanFormat, &props);

        const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        const VkFormatFeatureFlags flags = (tiling == VK_IMAGE_TILING_LINEAR)
            ? props.linearTilingFeatures
            : (tiling == VK_IMAGE_TILING_OPTIMAL) ? props.optimalTilingFeatures
                                                  : 0;

        return ((flags & featureFlags) == featureFlags);
    }

    /* get the first supported format out of the provided list of format choices. */
    TextureFormat FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
    {
        Assert(possibleFormats.Size() > 0, "Size must be greater than zero!");

        if (m_physicalDevice == nullptr)
        {
            return TF_NONE;
        }

        for (SizeType i = 0; i < possibleFormats.Size(); i++)
        {
            if (IsSupportedFormat(possibleFormats[i], supportType) != VK_FORMAT_UNDEFINED)
            {
                return possibleFormats[i];
            }
        }

        return TF_NONE;
    }

    /* get the first supported format out of the provided list of format choices. */
    template <class Predicate>
    TextureFormat FindSupportedSurfaceFormat(const VulkanSwapchainSupportDetails& details, Span<TextureFormat> possibleFormats, Predicate&& predicate) const
    {
        Assert(possibleFormats.Size() != 0, "Size must be greater than zero!");

        for (TextureFormat format : possibleFormats)
        {
            const VkFormat vkFormat = ToVkFormat(format);

            if (AnyOf(details.formats, [&](auto&& surfaceFormat)
                    {
                        return surfaceFormat.format == vkFormat && predicate(surfaceFormat);
                    }))
            {
                return format;
            }
        }

        return TF_NONE;
    }

    RendererResult GetImageFormatProperties(
        VkFormat format,
        VkImageType type,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkImageCreateFlags flags,
        VkImageFormatProperties* outProperties) const
    {
        if (m_physicalDevice == nullptr)
        {
            return HYP_MAKE_ERROR(RendererError, "Cannot find supported format; physical device is not initialized.");
        }

        VkResult vkResult;

        if ((vkResult = vkGetPhysicalDeviceImageFormatProperties(m_physicalDevice, format, type, tiling, usage, flags, outProperties)) != VK_SUCCESS)
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to get image format properties", vkResult);
        }

        HYPERION_RETURN_OK;
    }

    template <class StructType>
    constexpr uint32 PaddedSize() const
    {
        return PaddedSize<StructType>(uint32(m_properties.limits.minUniformBufferOffsetAlignment));
    }

    template <class StructType>
    constexpr uint32 PaddedSize(uint32 alignment) const
    {
        return PaddedSize(uint32(sizeof(StructType)), alignment);
    }

    constexpr uint32 PaddedSize(uint32 size, uint32 alignment) const
    {
        return alignment
            ? (size + alignment - 1) & ~(alignment - 1)
            : size;
    }

    bool SupportsGeometryShaders() const
    {
        return m_features.geometryShader;
    }

    bool IsRaytracingDisabled() const
    {
        return !IsRaytracingSupported() || m_isRaytracingDisabled;
    }

    void SetIsRaytracingDisabled(bool isRaytracingDisabled)
    {
        m_isRaytracingDisabled = isRaytracingDisabled;
    }

    bool IsRaytracingEnabled() const
    {
        return IsRaytracingSupported() && !m_isRaytracingDisabled;
    }

    bool IsRaytracingSupported() const
    {
#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        return m_raytracingPipelineFeatures.rayTracingPipeline
            && m_accelerationStructureFeatures.accelerationStructure
            && m_bufferDeviceAddressFeatures.bufferDeviceAddress;
#else
        return false;
#endif
    }

private:
    VkPhysicalDevice m_physicalDevice;
    VkPhysicalDeviceProperties m_properties;
    VkPhysicalDeviceFeatures m_features;

    VkPhysicalDeviceBufferDeviceAddressFeatures m_bufferDeviceAddressFeatures;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_raytracingPipelineFeatures;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_raytracingPipelineProperties;
    VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT m_samplerMinmaxProperties;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelerationStructureFeatures;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_accelerationStructureProperties;
    ;
    VkPhysicalDeviceDescriptorIndexingFeatures m_indexingFeatures;
    VkPhysicalDeviceDescriptorIndexingProperties m_indexingProperties;
    VkPhysicalDeviceMultiviewFeatures m_multiviewFeatures;
    VkPhysicalDeviceFeatures2 m_features2;
    VkPhysicalDeviceProperties2 m_properties2;

    VkPhysicalDeviceMemoryProperties m_memoryProperties;

    Array<UniquePtr<VkBaseOutStructure>> m_featuresChain;

    bool m_isRaytracingDisabled { false };
};

} // namespace hyperion
