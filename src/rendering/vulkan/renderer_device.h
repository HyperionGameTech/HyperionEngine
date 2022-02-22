//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_DEVICE_H
#define HYPERION_RENDERER_DEVICE_H

#include <vector>
#include <set>
#include <optional>

#include "renderer_structs.h"

class RendererDevice {
public:
    RendererDevice();
    ~RendererDevice();

    void SetDevice(const VkDevice &_device);
    void SetPhysicalDevice(const VkPhysicalDevice &_physical);
    void SetRenderSurface(const VkSurfaceKHR &_surface);
    void SetRequiredExtensions(const std::vector<const char *> &_extensions);
    VkDevice         GetDevice();
    VkSurfaceKHR     GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();
    std::vector<const char *> GetRequiredExtensions();

    QueueFamilyIndices FindQueueFamilies();
    SwapchainSupportDetails QuerySwapchainSupport();

    VkQueue GetQueue(uint32_t queue_family_index, uint32_t queue_index=0);

    VkDevice CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions);
    VkPhysicalDeviceFeatures GetDeviceFeatures();

    bool CheckDeviceSuitable();

    std::vector<const char *> CheckExtensionSupport(std::vector<const char *> _extensions);
    std::vector<const char *> CheckExtensionSupport();

    std::vector<VkExtensionProperties> GetSupportedExtensions();
private:
    VkDevice         device;
    VkSurfaceKHR     surface;
    VkPhysicalDevice physical;

    std::vector<const char *> required_extensions;
};


#endif //HYPERION_RENDERER_DEVICE_H
