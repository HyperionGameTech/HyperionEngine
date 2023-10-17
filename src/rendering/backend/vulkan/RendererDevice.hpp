#ifndef HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP

#include <vector>
#include <set>
#include <unordered_map>
#include <string>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <system/vma/VmaUsage.hpp>

namespace hyperion {
namespace renderer {

class Features;


using ExtensionMap = std::unordered_map<std::string, bool>;

namespace platform {

template <PlatformType PLATFORM>
class Instance;

template <>
class Device<Platform::VULKAN>
{
    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

public:
    Device(VkPhysicalDevice physical, VkSurfaceKHR surface);
    ~Device();

    void Destroy();
    
    void SetPhysicalDevice(VkPhysicalDevice);
    void SetRenderSurface(const VkSurfaceKHR &surface);
    void SetRequiredExtensions(const ExtensionMap &extensions);

    VkDevice GetDevice();
    VkSurfaceKHR GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();

    void DebugLogAllocatorStats() const;

    Result SetupAllocator(Instance<Platform::VULKAN> *instance);
    Result DestroyAllocator();
    VmaAllocator GetAllocator() const { return allocator; }

    const QueueFamilyIndices &GetQueueFamilyIndices() const { return queue_family_indices; }
    const Features &GetFeatures() const { return *features; }

    VkQueue GetQueue(UInt32 queue_family_index, UInt32 queue_index = 0);

    Result CreateLogicalDevice(const std::set<uint32_t> &required_queue_families);
    Result CheckDeviceSuitable(const ExtensionMap &unsupported_extensions);

    /*  Wait for the device to be idle */
    Result Wait() const;
    
    /* \brief Check if the set required extensions extensions are supported. Any unsupported extensions are returned. */
    ExtensionMap GetUnsupportedExtensions();

    std::vector<VkExtensionProperties> GetSupportedExtensions();

private:
    VkDevice device;
    VkPhysicalDevice physical;
    VkSurfaceKHR surface;
    VmaAllocator allocator;

    Features *features;
    QueueFamilyIndices queue_family_indices;

    ExtensionMap required_extensions;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_DEVICE_HPP
