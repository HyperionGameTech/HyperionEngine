//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>

#include <vulkan/vulkan.hpp>
#include "../system/sdl_system.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

class RendererDevice {
public:
    void SetDevice(const VkDevice &_device);
    void SetPhysicalDevice(const VkPhysicalDevice &_physical);
    VkDevice         GetDevice();
    VkPhysicalDevice GetPhysicalDevice();

    VkDevice Initialize(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions);
private:
    VkDevice         device;
    VkPhysicalDevice physical;
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

class VkRenderer {
    static bool CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
public:
    VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name);
    void SetupDebug();
    VkDevice InitDevice(const VkPhysicalDevice &physical,
                                    std::set<uint32_t> unique_queue_families,
                                    const std::vector<const char *> &required_extensions);

    void SetValidationLayers(std::vector<const char *> _layers);

    void SetCurrentWindow(const SystemWindow &window);
    SystemWindow GetCurrentWindow();

    ~VkRenderer();

private:
    SystemWindow window = SystemWindow(nullptr, 0, 0);

    VkInstance instance;
    VkSurfaceKHR surface;

    VkSemaphore sp_image_available;
    VkSemaphore sp_render_finished;

    VkQueue queue_graphics;
    VkQueue queue_present;

    RendererDevice device;

    std::vector<const char *> validation_layers;
};




#endif //HYPERION_VK_RENDERER_H
