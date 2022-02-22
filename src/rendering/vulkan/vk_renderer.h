//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <vulkan/vulkan.h>

#include "../../system/sdl_system.h"

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"
#include "renderer_pipeline.h"

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

namespace hyperion {

class RendererQueue {
public:
    RendererQueue();
    void GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index, uint32_t queue_index=0);
private:
    VkQueue queue;
};

class VkRenderer {
    static bool CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);
    void CreateSyncObjects();
    void SetupDebug();
    VkResult AcquireNextImage(uint32_t *image_index);
public:
    VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name);
    void Initialize(bool load_debug_layers=false);
    void CreateSurface();

    void RenderFrame(uint32_t *image_index);
    void DrawFrame();

    void SetValidationLayers(std::vector<const char *> _layers);
    RendererDevice *GetRendererDevice();
    RendererDevice *InitializeRendererDevice(VkPhysicalDevice _physical_device=nullptr);
    void InitializeSwapchain();
    void InitializePipeline(RendererShader *shader);

    void SetQueueFamilies(std::set<uint32_t> queue_families);

    void SetCurrentWindow(SystemWindow *window);
    SystemWindow *GetCurrentWindow();
    void Destroy();

    std::vector<const char *> requested_device_extensions;
    const char *app_name;
    const char *engine_name;
private:
    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    VkSemaphore sp_swap_acquire;
    VkSemaphore sp_swap_release;
    VkFence     fc_queue_submit;


    VkQueue queue_graphics;
    VkQueue queue_present;

    RendererDevice    *device = nullptr;
    RendererSwapchain *swapchain = nullptr;
    RendererPipeline  *pipeline = nullptr;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;
};

} // namespace hyperion


#endif //HYPERION_VK_RENDERER_H
