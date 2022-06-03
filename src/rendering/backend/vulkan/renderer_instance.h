#ifndef HYPERION_RENDERER_INSTANCE_H
#define HYPERION_RENDERER_INSTANCE_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <vulkan/vulkan.h>

#include <system/sdl_system.h>
#include <system/vma/vma_usage.h>

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"
#include "renderer_buffer.h"
#include "renderer_graphics_pipeline.h"
#include "renderer_descriptor_set.h"
#include "renderer_frame.h"
#include "renderer_frame_handler.h"
#include "renderer_queue.h"

#include <util/defines.h>

namespace hyperion {
namespace renderer {

class Instance {
    static Result CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
    static ExtensionMap GetExtensionMap();

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

    Result CreateCommandPool(Queue &queue);

public:
    Instance(SystemSDL &_system, const char *app_name, const char *engine_name);
    Result Initialize(bool load_debug_layers = false);
    void CreateSurface();
    
    void WaitImageReady(Frame *frame);

    inline DescriptorPool &GetDescriptorPool()             { return this->descriptor_pool; }
    inline const DescriptorPool &GetDescriptorPool() const { return this->descriptor_pool; }

    inline Queue &GetGraphicsQueue()             { return this->queue_graphics; }
    inline const Queue &GetGraphicsQueue() const { return this->queue_graphics; }
    inline Queue &GetTransferQueue()             { return this->queue_transfer; }
    inline const Queue &GetTransferQueue() const { return this->queue_transfer; }
    inline Queue &GetPresentQueue()              { return this->queue_present; }
    inline const Queue &GetPresentQueue() const  { return this->queue_present; }
    inline Queue &GetComputeQueue()              { return this->queue_compute; }
    inline const Queue &GetComputeQueue() const  { return this->queue_compute; }

    inline VkCommandPool GetGraphicsCommandPool() const { return this->queue_graphics.command_pool; }
    inline VkCommandPool GetComputeCommandPool() const  { return this->queue_compute.command_pool; }
    
    inline VkInstance GetInstance() const { return this->instance; }

    void SetValidationLayers(std::vector<const char *> _layers);

    Device *GetDevice();
    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();

    void SetCurrentWindow(SystemWindow *window);

    inline Swapchain *GetSwapchain() { return swapchain; }
    inline const Swapchain *GetSwapchain() const { return swapchain; }
    
    inline FrameHandler *GetFrameHandler() { return frame_handler; }
    inline const FrameHandler *GetFrameHandler() const { return frame_handler; }

    inline VmaAllocator GetAllocator() const { return allocator; }

    SystemWindow *GetCurrentWindow();
    Result Destroy();

    helpers::SingleTimeCommands GetSingleTimeCommands();

    inline StagingBufferPool &GetStagingBufferPool() { return m_staging_buffer_pool; }
    inline const StagingBufferPool &GetStagingBufferPool() const { return m_staging_buffer_pool; }

    const char *app_name;
    const char *engine_name;
    
    Swapchain *swapchain = nullptr;

    /* Per frame data */
    FrameHandler *frame_handler;

private:

    SystemWindow *window = nullptr;
    SystemSDL    system;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    DescriptorPool descriptor_pool;

    VmaAllocator allocator = nullptr;

    Device    *device = nullptr;

    Queue queue_graphics,
          queue_transfer,
          queue_present,
          queue_compute;
    
    std::vector<const char *> validation_layers;

    StagingBufferPool m_staging_buffer_pool;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
};

} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE

