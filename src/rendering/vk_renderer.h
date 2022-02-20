//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_VK_RENDERER_H
#define HYPERION_VK_RENDERER_H

#include <vector>
#include <set>
#include <array>

#include "./backend/spirv.h"

#include <vulkan/vulkan.hpp>
#include "../system/sdl_system.h"
#include <optional>

#define VK_RENDERER_API_VERSION VK_API_VERSION_1_2

namespace hyperion {

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
    void CreateFramebuffers(VkRenderPass *renderpass);

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

struct RendererShaderModule {
    SPIRVObject::Type type;
    VkShaderModule module;
};

class RendererShader {
public:
    void AttachShader(RendererDevice *device, const SPIRVObject &spirv);
    //void AttachShader(RendererDevice *device, ShaderType type, const uint32_t *code, const size_t code_size);
    VkPipelineShaderStageCreateInfo CreateShaderStage(RendererShaderModule *module, const char *entry_point);
    void CreateProgram(const char *entry_point);
    void Destroy();

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
private:
    std::vector<RendererShaderModule> shader_modules;
    RendererDevice *device = nullptr;
};

class RendererPipeline {

public:
    RendererPipeline(RendererDevice *_device, RendererSwapchain *_swapchain);
    void SetPrimitive(VkPrimitiveTopology _primitive);
    void SetDynamicStates(const std::vector<VkDynamicState> &_states);
    VkPrimitiveTopology GetPrimitive();
    std::vector<VkDynamicState> GetDynamicStates();
    ~RendererPipeline();

    void SetViewport(float x, float y, float width, float height, float min_depth=0.0f, float max_depth=1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);

    void Rebuild(RendererShader *shader);
    void CreateRenderPass(VkSampleCountFlagBits sample_count=VK_SAMPLE_COUNT_1_BIT);
private:
    std::vector<VkDynamicState> dynamic_states;

    VkViewport viewport;
    VkRect2D   scissor;
    VkPrimitiveTopology primitive;

    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkRenderPass render_pass;

    RendererSwapchain *swapchain;
    RendererDevice *device;
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
    RendererDevice *GetRendererDevice();
    RendererDevice *InitializeRendererDevice(VkPhysicalDevice _physical_device=nullptr);
    void InitializeSwapchain();

    void InitializePipeline(RendererShader *shader);

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

    RendererDevice    *device = nullptr;
    RendererSwapchain *swapchain = nullptr;
    RendererPipeline  *pipeline = nullptr;

    std::set<uint32_t> queue_families;
    std::vector<const char *> validation_layers;
};

} // namespace hyperion


#endif //HYPERION_VK_RENDERER_H
