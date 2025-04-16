/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP

#include <set>
#include <unordered_map>
#include <string>

#include <core/memory/UniquePtr.hpp>
#include <core/containers/Array.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererQueue.hpp>

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

template <PlatformType PLATFORM>
class AsyncCompute;

template <>
class Device<Platform::VULKAN>
{
    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

public:
    static constexpr PlatformType platform = Platform::VULKAN;
    
    Device(VkPhysicalDevice physical, VkSurfaceKHR surface);
    Device(const Device &)                  = delete;
    Device &operator=(const Device &)       = delete;
    Device(Device &&) noexcept              = delete;
    Device &operator=(Device &&) noexcept   = delete;
    ~Device();

    void Destroy();
    
    void SetRenderSurface(const VkSurfaceKHR &surface);
    void SetRequiredExtensions(const ExtensionMap &extensions);

    VkDevice GetDevice();
    VkSurfaceKHR GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();

    void DebugLogAllocatorStats() const;

    RendererResult SetupAllocator(Instance<Platform::VULKAN> *instance);
    RendererResult DestroyAllocator();
    VmaAllocator GetAllocator() const { return m_allocator; }

    const QueueFamilyIndices &GetQueueFamilyIndices() const { return m_queue_family_indices; }
    const Features &GetFeatures() const { return *m_features; }
                                                          
    DeviceQueue<Platform::VULKAN> &GetGraphicsQueue() { return m_queue_graphics; }
    const DeviceQueue<Platform::VULKAN> &GetGraphicsQueue() const { return m_queue_graphics; }
    DeviceQueue<Platform::VULKAN> &GetTransferQueue() { return m_queue_transfer; }
    const DeviceQueue<Platform::VULKAN> &GetTransferQueue() const { return m_queue_transfer; }
    DeviceQueue<Platform::VULKAN> &GetPresentQueue() { return m_queue_present; }
    const DeviceQueue<Platform::VULKAN> &GetPresentQueue() const { return m_queue_present; }
    DeviceQueue<Platform::VULKAN> &GetComputeQueue() { return m_queue_compute; }
    const DeviceQueue<Platform::VULKAN> &GetComputeQueue() const { return m_queue_compute; }

    VkQueue GetQueue(uint32 queue_family_index, uint32 queue_index = 0);

    RendererResult Create(const std::set<uint32_t> &required_queue_families);
    RendererResult CheckDeviceSuitable(const ExtensionMap &unsupported_extensions);

    /*! \brief Wait for the device to be idle */
    RendererResult Wait() const;
    
    /*! \brief Check if the set required extensions extensions are supported. Any unsupported extensions are returned. */
    ExtensionMap GetUnsupportedExtensions();

    Array<VkExtensionProperties> GetSupportedExtensions();

private:
    VkDevice                                                m_device;
    VkPhysicalDevice                                        m_physical;
    VkSurfaceKHR                                            m_surface;
    VmaAllocator                                            m_allocator;

    UniquePtr<Features>                                     m_features;
    QueueFamilyIndices                                      m_queue_family_indices;

    DeviceQueue<Platform::VULKAN>                           m_queue_graphics;
    DeviceQueue<Platform::VULKAN>                           m_queue_transfer;
    DeviceQueue<Platform::VULKAN>                           m_queue_present;
    DeviceQueue<Platform::VULKAN>                           m_queue_compute;

    ExtensionMap                                            m_required_extensions;

    UniquePtr<DescriptorPool>                               m_descriptor_pool;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP
