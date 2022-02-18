//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>
#include <array>

#include <vulkan/vulkan.hpp>
#include "../system/sdl_system.h"
#include <optional>

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete() {
        return this->graphics_family.has_value() && this->present_family.has_value();
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

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

class RendererQueue {
public:
    RendererQueue();
    void GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index, uint32_t queue_index=0);
private:
    VkQueue queue;
};

class RendererSwapchain {
    VkSurfaceFormatKHR ChooseSurfaceFormat();
    VkPresentModeKHR   ChoosePresentMode();
    VkExtent2D         ChooseSwapchainExtent();
    void RetrieveImageHandles();
    void CreateImageView(size_t index, VkImage *swapchain_image);
    void CreateImageViews();
    void DestroyImageViews();

public:
    RendererSwapchain(RendererDevice *_device, const SwapchainSupportDetails &_details);
    ~RendererSwapchain();
    void Create(const VkSurfaceKHR &surface, QueueFamilyIndices qf_indices);

    VkSwapchainKHR swapchain;
    VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;

    VkFormat image_format;
private:
    RendererDevice *renderer_device = nullptr;
    SwapchainSupportDetails support_details;

    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkFramebuffer> framebuffers;
};

enum class ShaderType : int {
    Vertex = 0,
    Fragment,
    Geometry,
    Compute,
    /* Mesh shaders */
    Task,
    Mesh,
    /* Tesselation */
    TessControl,
    TessEval,
    /* Raytracing */
    RayGen,
    RayIntersect,
    RayAnyHit,
    RayClosestHit,
    RayMiss,
};

class RendererShader {
public:
    void AttachShader(RendererDevice *device, ShaderType type, const std::string &path);
    void AttachShader(RendererDevice *device, ShaderType type, const uint32_t *code, const size_t code_size);
private:
    std::vector<VkShaderModule> shader_modules;
};

class VkRenderer {
    static bool CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);
    void SetupDebug();
public:
    VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name);
    void Initialize(bool load_debug_layers=false);
    void CreateSurface();
    void SetValidationLayers(std::vector<const char *> _layers);
    void SetRendererDevice(RendererDevice *_device);
    RendererDevice *InitializeRendererDevice(VkPhysicalDevice _physical_device=nullptr);
    void InitializeSwapchain();

    void SetQueueFamilies(std::set<uint32_t> queue_families);

    void SetCurrentWindow(SystemWindow *window);
    SystemWindow *GetCurrentWindow();
    ~VkRenderer();

    std::vector<const char *> requested_device_extensions;
    const char *app_name;
    const char *engine_name;
private:
    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance;
    VkSurfaceKHR surface;

    VkSemaphore sp_image_available;
    VkSemaphore sp_render_finished;

    VkQueue queue_graphics;
    VkQueue queue_present;

    RendererSwapchain *swapchain;
    RendererDevice    *device;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;
};




#endif //HYPERION_VK_RENDERER_H
