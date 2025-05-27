/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP

#include <set>
#include <unordered_map>
#include <string>

#include <core/memory/UniquePtr.hpp>
#include <core/containers/Array.hpp>

#include <rendering/backend/vulkan/RendererQueue.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <system/vma/VmaUsage.hpp>

namespace hyperion {
namespace renderer {

class Features;
class DescriptorPool;

using ExtensionMap = std::unordered_map<std::string, bool>;

namespace platform {

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class DescriptorSetManager;

template <>
class Device<Platform::vulkan> final
{
    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

public:
    static constexpr PlatformType platform = Platform::vulkan;

    Device(VkPhysicalDevice physical, VkSurfaceKHR surface);
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept = delete;
    Device& operator=(Device&&) noexcept = delete;
    ~Device();

    void Destroy();

    void SetRenderSurface(const VkSurfaceKHR& surface);
    void SetRequiredExtensions(const ExtensionMap& extensions);

    VkDevice GetDevice();
    VkSurfaceKHR GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();

    void DebugLogAllocatorStats() const;

    RendererResult SetupAllocator(Instance<Platform::vulkan>* instance);
    RendererResult DestroyAllocator();

    VmaAllocator GetAllocator() const
    {
        return m_allocator;
    }

    const QueueFamilyIndices& GetQueueFamilyIndices() const
    {
        return m_queue_family_indices;
    }

    const Features& GetFeatures() const
    {
        return *m_features;
    }

    VulkanDeviceQueue& GetGraphicsQueue()
    {
        return m_queue_graphics;
    }

    const VulkanDeviceQueue& GetGraphicsQueue() const
    {
        return m_queue_graphics;
    }

    VulkanDeviceQueue& GetTransferQueue()
    {
        return m_queue_transfer;
    }

    const VulkanDeviceQueue& GetTransferQueue() const
    {
        return m_queue_transfer;
    }

    VulkanDeviceQueue& GetPresentQueue()
    {
        return m_queue_present;
    }

    const VulkanDeviceQueue& GetPresentQueue() const
    {
        return m_queue_present;
    }

    VulkanDeviceQueue& GetComputeQueue()
    {
        return m_queue_compute;
    }

    const VulkanDeviceQueue& GetComputeQueue() const
    {
        return m_queue_compute;
    }

    VkQueue GetQueue(uint32 queue_family_index, uint32 queue_index = 0);

    RendererResult Create(const std::set<uint32_t>& required_queue_families);
    RendererResult CheckDeviceSuitable(const ExtensionMap& unsupported_extensions);

    /*! \brief Wait for the device to be idle */
    RendererResult Wait() const;

    /*! \brief Check if the set required extensions extensions are supported. Any unsupported extensions are returned. */
    ExtensionMap GetUnsupportedExtensions();

    Array<VkExtensionProperties> GetSupportedExtensions();

private:
    VkDevice m_device;
    VkPhysicalDevice m_physical;
    VkSurfaceKHR m_surface;
    VmaAllocator m_allocator;

    UniquePtr<Features> m_features;
    QueueFamilyIndices m_queue_family_indices;

    VulkanDeviceQueue m_queue_graphics;
    VulkanDeviceQueue m_queue_transfer;
    VulkanDeviceQueue m_queue_present;
    VulkanDeviceQueue m_queue_compute;

    ExtensionMap m_required_extensions;

    UniquePtr<DescriptorPool> m_descriptor_pool;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP
