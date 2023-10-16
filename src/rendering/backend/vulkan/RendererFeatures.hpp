#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FEATURES_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FEATURES_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/lib/UniquePtr.hpp>
#include <core/lib/DynArray.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

#include <array>

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK
    #include <MoltenVK/vk_mvk_moltenvk.h>
#endif

namespace hyperion {
namespace renderer {

class Features
{
public:
    struct DynamicFunctions
    {
#define HYP_DECL_FN(name) PFN_##name name = nullptr

        HYP_DECL_FN(vkGetBufferDeviceAddressKHR);
        HYP_DECL_FN(vkCmdBuildAccelerationStructuresKHR);
        HYP_DECL_FN(vkBuildAccelerationStructuresKHR);
        HYP_DECL_FN(vkCreateAccelerationStructureKHR);
        HYP_DECL_FN(vkDestroyAccelerationStructureKHR);
        HYP_DECL_FN(vkGetAccelerationStructureBuildSizesKHR);
        HYP_DECL_FN(vkGetAccelerationStructureDeviceAddressKHR);
        HYP_DECL_FN(vkCmdTraceRaysKHR);
        HYP_DECL_FN(vkGetRayTracingShaderGroupHandlesKHR);
        HYP_DECL_FN(vkCreateRayTracingPipelinesKHR);

        //debugging
        HYP_DECL_FN(vkCmdDebugMarkerBeginEXT);
        HYP_DECL_FN(vkCmdDebugMarkerEndEXT);
        HYP_DECL_FN(vkCmdDebugMarkerInsertEXT);
        HYP_DECL_FN(vkDebugMarkerSetObjectNameEXT);

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
        HYP_DECL_FN(vkGetMoltenVKConfigurationMVK);
        HYP_DECL_FN(vkSetMoltenVKConfigurationMVK);
#endif

#undef HYP_DECL_FN
    };

    static DynamicFunctions dyn_functions;

    Features();
    Features(VkPhysicalDevice);

    Features(const Features &other) = delete;
    Features &operator=(const Features &other) = delete;
    ~Features() = default;

    void SetPhysicalDevice(VkPhysicalDevice);

    VkPhysicalDevice GetPhysicalDevice() const
        { return m_physical_device; }

    bool IsDiscreteGpu() const
        { return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; }

    const char *GetDeviceName() const
        { return m_properties.deviceName; }

    const VkPhysicalDeviceProperties &GetPhysicalDeviceProperties() const
        { return m_properties; }

    const VkPhysicalDeviceFeatures &GetPhysicalDeviceFeatures() const
        { return m_features; }

    const VkPhysicalDeviceFeatures2 &GetPhysicalDeviceFeatures2() const
        { return m_features2; }

    const VkPhysicalDeviceDescriptorIndexingFeatures &GetPhysicalDeviceIndexingFeatures() const
        { return m_indexing_features; }

    const VkPhysicalDeviceMemoryProperties &GetPhysicalDeviceMemoryProperties() const
        { return m_memory_properties; }

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &GetRaytracingPipelineFeatures() const
        { return m_raytracing_pipeline_features; }

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &GetRaytracingPipelineProperties() const
        { return m_raytracing_pipeline_properties; }

    const VkPhysicalDeviceBufferDeviceAddressFeatures &GetBufferDeviceAddressFeatures() const
        { return m_buffer_device_address_features; }

    const VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT &GetSamplerMinMaxProperties() const
        { return m_sampler_minmax_properties; }

    const VkPhysicalDeviceAccelerationStructureFeaturesKHR &GetAccelerationStructureFeatures() const
        { return m_acceleration_structure_features; }

    const VkPhysicalDeviceAccelerationStructurePropertiesKHR &GetAccelerationStructureProperties() const
        { return m_acceleration_structure_properties; }

    struct DeviceRequirementsResult
    {
        enum
        {
            DEVICE_REQUIREMENTS_OK = 0,
            DEVICE_REQUIREMENTS_ERR = 1
        } result;

        const char *message;

        DeviceRequirementsResult(decltype(result) result, const char *message = "")
            : result(result), message(message) {}
        DeviceRequirementsResult(const DeviceRequirementsResult &other)
            : result(other.result), message(other.message) {}
        operator bool() const { return result == DEVICE_REQUIREMENTS_OK; }
    };

#define REQUIRES_VK_FEATURE_MSG(cond, feature) \
    do { \
        if (!(cond)) { \
            return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "Feature constraint '" #feature "' not satisfied."); \
        } \
    } while (0)

#define REQUIRES_VK_FEATURE(cond) \
    do { \
        if (!(cond)) { \
            return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "Feature constraint '" #cond "' not satisfied."); \
        } \
    } while (0)

    DeviceRequirementsResult SatisfiesMinimumRequirements()
    {
        REQUIRES_VK_FEATURE_MSG(m_features.fragmentStoresAndAtomics, "Image stores and atomics in fragment shaders"); /* for imageStore() in fragment shaders */
        REQUIRES_VK_FEATURE_MSG(m_features.shaderSampledImageArrayDynamicIndexing, "Dynamic sampler / image array indexing"); /* for accessing textures based on dynamic index (push constant) */
        // REQUIRES_VK_FEATURE_MSG(m_indexing_features.descriptorBindingPartiallyBound && m_indexing_features.runtimeDescriptorArray, "Bindless descriptors"); /* bindless support */
        REQUIRES_VK_FEATURE_MSG(m_multiview_features.multiview, "Multiview not supported");
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetSamplers >= 16);
        REQUIRES_VK_FEATURE(m_properties.limits.maxDescriptorSetUniformBuffers >= 16);

#if HYP_FEATURES_BINDLESS_TEXTURES
        REQUIRES_VK_FEATURE(m_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers >= 4096);
#else
        REQUIRES_VK_FEATURE(m_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers >= 16);
#endif

        return DeviceRequirementsResult(DeviceRequirementsResult::DEVICE_REQUIREMENTS_OK);
    }

#undef REQUIRES_VK_FEATURE

    bool SupportsBindlessTextures() const
    {
#if !HYP_FEATURES_BINDLESS_TEXTURES
        return false;
#else

        return m_indexing_features.descriptorBindingPartiallyBound
            && m_indexing_features.runtimeDescriptorArray
            && m_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers >= 4096
            && m_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages >= 4096;
#endif
    }

    void LoadDynamicFunctions(Device *device);
    void SetDeviceFeatures(Device *device);

    SwapchainSupportDetails QuerySwapchainSupport(VkSurfaceKHR _surface) const
    {
        SwapchainSupportDetails details { };

        if (m_physical_device == nullptr) {
            DebugLog(LogType::Debug, "No physical device set -- cannot query swapchain support!\n");
            return details;
        }

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, _surface, &details.capabilities);

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_family_properties.data());

        uint32_t count = 0;
        /* Get device surface formats */
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, _surface, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> surface_formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, _surface, &count, surface_formats.data());
        if (count == 0)
            DebugLog(LogType::Warn, "No surface formats available!\n");

        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, _surface, &count, nullptr);
        std::vector<VkPresentModeKHR> present_modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, _surface, &count, present_modes.data());
        if (count == 0)
            DebugLog(LogType::Warn, "No present modes available!\n");

        details.queue_family_properties = queue_family_properties;
        details.formats = surface_formats;
        details.present_modes = present_modes;

        return details;
    }

    bool IsSupportedFormat(
        InternalFormat format,
        ImageSupportType support_type
    ) const
    {
        if (m_physical_device == nullptr) {
            return false;
        }

        DebugLog(
            LogType::Debug,
            "Checking support for format %d with support_type %d.\n",
            int(format),
            int(support_type)
        );

        const VkFormat vulkan_format = helpers::ToVkFormat(format);

        VkFormatFeatureFlags feature_flags = 0;

        switch (support_type) {
        case ImageSupportType::SRV:
            feature_flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
            break;
        case ImageSupportType::UAV:
            feature_flags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
            break;
        case ImageSupportType::DEPTH:
            feature_flags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        default:
            DebugLog(
                LogType::RenError,
                "Unknown image support type %d.\n",
                int(support_type)
            );

            return false;
        }

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physical_device, vulkan_format, &props);

        const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        const VkFormatFeatureFlags flags = (tiling == VK_IMAGE_TILING_LINEAR)
            ? props.linearTilingFeatures
            : (tiling == VK_IMAGE_TILING_OPTIMAL) ? props.optimalTilingFeatures : 0;

        if ((flags & feature_flags) == feature_flags) {
            DebugLog(
                LogType::Debug,
                "Vulkan format %d with tiling mode %d and feature flags %d support found!\n",
                format,
                tiling,
                feature_flags
            );

            return true;
        }

        DebugLog(
            LogType::Debug,
            "Vulkan format %d with tiling mode %d and feature flags %d not supported.\n",
            format,
            tiling,
            feature_flags
        );

        return false;
    }

    /* get the first supported format out of the provided list of format choices. */
    template <SizeType Size>
    InternalFormat FindSupportedFormat(
        const std::array<InternalFormat, Size> &possible_formats,
        ImageSupportType support_type
    ) const
    {
        static_assert(Size > 0, "Size must be greater than zero!");

        DebugLog(
            LogType::Debug,
            "Looking for format to use with support type %d. First choice: %d\n",
            int(support_type),
            int(possible_formats[0])
        );

        if (m_physical_device == nullptr) {
            DebugLog(LogType::Debug, "No physical device set -- cannot find supported format!\n");
            return InternalFormat::NONE;
        }

        for (SizeType i = 0; i < Size; i++) {
            if (IsSupportedFormat(possible_formats[i], support_type) != VK_FORMAT_UNDEFINED) {
                return possible_formats[i];
            }
        }

        return InternalFormat::NONE;
    }

    /* get the first supported format out of the provided list of format choices. */
    template <SizeType Size, class LambdaFunction>
    InternalFormat FindSupportedSurfaceFormat(
        const SwapchainSupportDetails &details,
        const std::array<InternalFormat, Size> &possible_formats,
        LambdaFunction predicate
    ) const
    {
        static_assert(Size > 0, "Size must be greater than zero!");

        DebugLog(
            LogType::Debug,
            "Looking for format to use for surface. First choice: %d\n",
            possible_formats[0]
        );

        DebugLog(LogType::Debug, "Available options:\n");

        for (const VkSurfaceFormatKHR &surface_format : details.formats) {
            DebugLog(
                LogType::Debug,
                "\tFormat: %d\tColor space: %d\n",
                surface_format.format,
                surface_format.colorSpace
            );
        }

        for (auto wanted_format : possible_formats) {
            DebugLog(
                LogType::Debug,
                "Try format: %d\n",
                wanted_format
            );

            const VkFormat wanted_vk_format = helpers::ToVkFormat(wanted_format);

            if (std::any_of(
                details.formats.begin(),
                details.formats.end(),
                [wanted_vk_format, &predicate](const VkSurfaceFormatKHR &surface_format) {
                    return surface_format.format == wanted_vk_format && predicate(surface_format);
                }
            )) {
                DebugLog(
                    LogType::Debug,
                    "Found surface format: %d\n",
                    wanted_format
                );

                return wanted_format;
            }
        }

        DebugLog(
            LogType::Debug,
            "No surface format found out of the selected options!\n"
        );

        return InternalFormat::NONE;
    }

    Result GetImageFormatProperties(
        VkFormat format,
        VkImageType type,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkImageCreateFlags flags,
        VkImageFormatProperties *out_properties
    ) const
    {
        if (m_physical_device == nullptr) {
            return Result(Result::RENDERER_ERR, "Cannot find supported format; physical device is not initialized.");
        }

        if (vkGetPhysicalDeviceImageFormatProperties(m_physical_device, format, type, tiling, usage, flags, out_properties) != VK_SUCCESS) {
            return Result(Result::RENDERER_ERR, "Failed to get image format properties");
        }

        HYPERION_RETURN_OK;
    }

    template <class StructType>
    constexpr UInt32 PaddedSize() const
    {
        return PaddedSize<StructType>(UInt32(m_properties.limits.minUniformBufferOffsetAlignment));
    }

    template <class StructType>
    constexpr UInt32 PaddedSize(UInt32 alignment) const
    {
        return PaddedSize(UInt32(sizeof(StructType)), alignment);
    }
    
    constexpr UInt32 PaddedSize(UInt32 size, UInt32 alignment) const
    {
        return alignment
            ? (size + alignment - 1) & ~(alignment - 1)
            : size;
    }

    bool SupportsGeometryShaders() const { return m_features.geometryShader; }

    bool IsRaytracingDisabled() const { return !IsRaytracingSupported() || m_is_raytracing_disabled; }
    void SetIsRaytracingDisabled(bool is_raytracing_disabled) { m_is_raytracing_disabled = is_raytracing_disabled; }

    bool IsRaytracingEnabled() const { return IsRaytracingSupported() && !m_is_raytracing_disabled; }

    bool IsRaytracingSupported() const
    {
#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && HYP_FEATURES_ENABLE_RAYTRACING && HYP_FEATURES_BINDLESS_TEXTURES
        return m_raytracing_pipeline_features.rayTracingPipeline
            && m_acceleration_structure_features.accelerationStructure
            && m_buffer_device_address_features.bufferDeviceAddress;
#else
        return false;
#endif
    }

private:
    VkPhysicalDevice m_physical_device;
    VkPhysicalDeviceProperties m_properties;
    VkPhysicalDeviceFeatures m_features;

    VkPhysicalDeviceBufferDeviceAddressFeatures m_buffer_device_address_features;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_raytracing_pipeline_features;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_raytracing_pipeline_properties;
    VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT m_sampler_minmax_properties;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_acceleration_structure_features;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_acceleration_structure_properties;;
    VkPhysicalDeviceDescriptorIndexingFeatures m_indexing_features;
    VkPhysicalDeviceDescriptorIndexingProperties m_indexing_properties;
    VkPhysicalDeviceMultiviewFeatures m_multiview_features;
    VkPhysicalDeviceFeatures2 m_features2;
    VkPhysicalDeviceProperties2 m_properties2;

    VkPhysicalDeviceMemoryProperties m_memory_properties;

    Array<UniquePtr<VkBaseOutStructure>> m_features_chain;

    bool m_is_raytracing_disabled { false };
};

} // namespace renderer
} // namespace hyperion

#endif
