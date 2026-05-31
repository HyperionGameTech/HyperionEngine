/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once
#include <Rendering/vulkan/VulkanResult.hpp>
#include <Rendering/vulkan/VulkanGpuImage.hpp>
#include <Rendering/vulkan/VulkanStructs.hpp>
#include <Rendering/vulkan/VulkanHelpers.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/containers/Array.hpp>

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <vulkan/vulkan.h>

#include <array>

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK
#include <MoltenVK/vk_mvk_moltenvk.h>
#endif

namespace Hyperion {

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

    bool IsIntegratedGpu() const
    {
        return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    }

    const char* GetDeviceName() const
    {
        return m_properties.deviceName;
    }

    uint32 GetDeviceId() const
    {
        return m_properties.deviceID;
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

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& GetRayTracingPipelineFeatures() const
    {
        return m_rayTracingPipelineFeatures;
    }

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRayTracingPipelineProperties() const
    {
        return m_rayTracingPipelineProperties;
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

    bool SupportsRayQueries() const
    {
        return m_rayQueryFeatures.rayQuery;
    }

    const VkPhysicalDeviceScalarBlockLayoutFeatures& GetScalarBlockLayoutFeatures() const
    {
        return m_scalarBlockLayoutFeatures;
    }

    bool SupportsScalarBlockLayout() const
    {
        return m_scalarBlockLayoutFeatures.scalarBlockLayout == VK_TRUE;
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

    VulkanSwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR surface) const
    {
        Assert(m_physicalDevice != VK_NULL_HANDLE, "No physical device set!");

        VulkanSwapchainSupportDetails details {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, surface, &details.capabilities);

        uint32 numQueueFamilies = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &numQueueFamilies, nullptr);

        Array<VkQueueFamilyProperties> queueFamilyProperties;
        queueFamilyProperties.Resize(numQueueFamilies);

        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &numQueueFamilies, queueFamilyProperties.Data());

        uint32 numSurfaceFormats = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, &numSurfaceFormats, nullptr);

        Array<VkSurfaceFormatKHR> surfaceFormats;
        surfaceFormats.Resize(numSurfaceFormats);

        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, &numSurfaceFormats, surfaceFormats.Data());
        Assert(surfaceFormats.Any(), "No surface formats available!");

        uint32 numPresentModes = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, surface, &numPresentModes, nullptr);

        Array<VkPresentModeKHR> presentModes;
        presentModes.Resize(numPresentModes);

        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, surface, &numPresentModes, presentModes.Data());
        Assert(presentModes.Any(), "No present modes available!");

        details.queueFamilyProperties = queueFamilyProperties;
        details.formats = surfaceFormats;
        details.presentModes = presentModes;

        return details;
    }

    bool IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
    {
        if (m_physicalDevice == nullptr)
        {
            return false;
        }

        const VkFormat vulkanFormat = ToVkFormat(format);

        VkFormatFeatureFlags featureFlags = 0;

        switch (supportType)
        {
        case ImageSupport::Attachment:
            if (TextureUtils::IsDepthFormat(format))
            {
                featureFlags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            else
            {
                featureFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
            }

            break;
        case ImageSupport::ShaderResource:
            featureFlags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            break;
        case ImageSupport::UnorderedAccess:
            featureFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
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
            return InvalidTextureFormat;
        }

        for (size_t i = 0; i < possibleFormats.Size(); i++)
        {
            if (IsSupportedFormat(possibleFormats[i], supportType) != VK_FORMAT_UNDEFINED)
            {
                return possibleFormats[i];
            }
        }

        return InvalidTextureFormat;
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

        return InvalidTextureFormat;
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

        return {};
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

    bool IsRayTracingDisabled() const
    {
        return !IsRayTracingSupported() || m_isRayTracingDisabled;
    }

    void SetIsRayTracingDisabled(bool isRayTracingDisabled)
    {
        m_isRayTracingDisabled = isRayTracingDisabled;
    }

    bool IsRayTracingEnabled() const
    {
        return IsRayTracingSupported() && !m_isRayTracingDisabled;
    }

    bool IsRayTracingSupported() const
    {
#if defined(HYP_FEATURES_ENABLE_RAY_TRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        return m_rayTracingPipelineFeatures.rayTracingPipeline
            && m_rayQueryFeatures.rayQuery
            && m_accelerationStructureFeatures.accelerationStructure
            && m_bufferDeviceAddressFeatures.bufferDeviceAddress;
#else
        return false;
#endif
    }

    const VkPhysicalDeviceVulkan12Features& GetVulkan12Features() const
    {
        return m_vulkan12Features;
    }

    // Timeline semaphores are mandatory in Vulkan 1.2. We target VK_API_VERSION_1_2
    // so they are always available on a compatible device.
    bool SupportsTimelineSemaphores() const
    {
        return m_vulkan12Features.timelineSemaphore == VK_TRUE;
    }

    bool SupportsExtendedDynamicState() const
    {
        return m_extendedDynamicStateFeatures.extendedDynamicState == VK_TRUE;
    }

private:
    VkPhysicalDevice m_physicalDevice;
    VkPhysicalDeviceProperties m_properties;
    VkPhysicalDeviceFeatures m_features;

    VkPhysicalDeviceBufferDeviceAddressFeatures m_bufferDeviceAddressFeatures;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rayTracingPipelineFeatures;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties;
    VkPhysicalDeviceRayQueryFeaturesKHR m_rayQueryFeatures;
    VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT m_samplerMinmaxProperties;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelerationStructureFeatures;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_accelerationStructureProperties;

    VkPhysicalDeviceDescriptorIndexingFeatures m_indexingFeatures;
    VkPhysicalDeviceDescriptorIndexingProperties m_indexingProperties;
    VkPhysicalDeviceVulkan12Features m_vulkan12Features;
    VkPhysicalDeviceMultiviewFeatures m_multiviewFeatures;
    VkPhysicalDeviceScalarBlockLayoutFeatures m_scalarBlockLayoutFeatures;
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT m_extendedDynamicStateFeatures;

    VkPhysicalDeviceFeatures2 m_features2;
    VkPhysicalDeviceProperties2 m_properties2;

    VkPhysicalDeviceMemoryProperties m_memoryProperties;

    bool m_isRayTracingDisabled { false };
};

} // namespace Hyperion
