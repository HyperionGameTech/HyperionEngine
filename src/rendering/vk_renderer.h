//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>

#include <vulkan/vulkan.hpp>
#include "../system/sdl_system.h"
#include <optional>

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

class RendererDevice {
public:
    RendererDevice();
    ~RendererDevice();

    void SetDevice(const VkDevice &_device);
    void SetPhysicalDevice(const VkPhysicalDevice &_physical);
    VkDevice         GetDevice();
    VkPhysicalDevice GetPhysicalDevice();
    VkQueue          GetQueue(uint32_t queue_family_index, uint32_t queue_index=0);

    VkDevice CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions);
    VkPhysicalDeviceFeatures GetDeviceFeatures();
    std::vector<VkExtensionProperties> CheckExtensionSupport(std::vector<const char *> required_extensions);
    std::vector<VkExtensionProperties> GetSupportedExtensions();
private:
    VkDevice         device;

    VkPhysicalDevice physical;
};

class RendererQueue {
public:
    RendererQueue();
    void GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index, uint32_t queue_index=0);
private:
    VkQueue queue;
};


class VkSwapchain {
public:
    VkSwapchain();
    void Create();

    VkSwapchainKHR swapchain;
private:
    VkExtent2D extent;
    VkFormat image_format;

    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkFramebuffer> framebuffers;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete() {
        return this->graphics_family.has_value() && this->present_family.has_value();
    }
};

class VkRenderer {
    static bool CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
    uint32_t    FindQueueFamily(VkPhysicalDevice _device);
    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);
    void SetupDebug();
public:
    VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name);
    void Initialize(bool load_debug_layers=false);
    void CreateSurface();
    void SetValidationLayers(std::vector<const char *> _layers);
    void SetRendererDevice(const RendererDevice &_device);
    RendererDevice *InitializeRendererDevice(VkPhysicalDevice _physical_device=nullptr);

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice _physical_device=nullptr);
    void SetQueueFamilies(std::set<uint32_t> queue_families);

    void SetCurrentWindow(SystemWindow *window);
    SystemWindow *GetCurrentWindow();
    ~VkRenderer();

    std::vector<const char *> requested_device_extensions;
    const char *app_name;
    const char *engine_name;
private:
    SystemWindow *window = nullptr;
    SystemSDL system;

    VkInstance instance;
    VkSurfaceKHR surface;

    VkSemaphore sp_image_available;
    VkSemaphore sp_render_finished;

    VkQueue queue_graphics;
    VkQueue queue_present;

    RendererDevice device;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;
};




#endif //HYPERION_VK_RENDERER_H
