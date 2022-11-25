#ifndef HYPERION_RENDERER_INSTANCE_H
#define HYPERION_RENDERER_INSTANCE_H

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <core/Containers.hpp>

#include <vulkan/vulkan.h>

#include <system/SdlSystem.hpp>
#include <system/vma/VmaUsage.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererQueue.hpp>

#include <util/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class Instance
{
    static Result CheckValidationLayerSupport(const std::vector<const char *> &requested_layers);
    static ExtensionMap GetExtensionMap();

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

    Result CreateCommandPool(DeviceQueue &queue, UInt index);

public:
    Instance(RefCountedPtr<Application> application);
    Result Initialize(bool load_debug_layers = false);
    void CreateSurface();
    
    DescriptorPool &GetDescriptorPool() { return this->descriptor_pool; }
    const DescriptorPool &GetDescriptorPool() const { return this->descriptor_pool; }
                                                          
    DeviceQueue &GetGraphicsQueue() { return this->queue_graphics; }
    const DeviceQueue &GetGraphicsQueue() const { return this->queue_graphics; }
    DeviceQueue &GetTransferQueue() { return this->queue_transfer; }
    const DeviceQueue &GetTransferQueue() const { return this->queue_transfer; }
    DeviceQueue &GetPresentQueue() { return this->queue_present; }
    const DeviceQueue &GetPresentQueue() const { return this->queue_present; }
    DeviceQueue &GetComputeQueue() { return this->queue_compute; }
    const DeviceQueue &GetComputeQueue() const { return this->queue_compute; }
                                                          
    VkCommandPool GetGraphicsCommandPool(UInt index = 0) const { return this->queue_graphics.command_pools[index]; }
    VkCommandPool GetComputeCommandPool(UInt index = 0) const { return this->queue_compute.command_pools[index]; }
    
    VkInstance GetInstance() const { return this->instance; }

    Swapchain *GetSwapchain() { return swapchain; }
    const Swapchain *GetSwapchain() const { return swapchain; }
                                                          
    FrameHandler *GetFrameHandler() { return frame_handler; }
    const FrameHandler *GetFrameHandler() const { return frame_handler; }

    StagingBufferPool &GetStagingBufferPool() { return m_staging_buffer_pool; }
    const StagingBufferPool &GetStagingBufferPool() const { return m_staging_buffer_pool; }

    VmaAllocator GetAllocator() const { return allocator; }

    helpers::SingleTimeCommands GetSingleTimeCommands();

    void SetValidationLayers(std::vector<const char *> _layers);

    Device *GetDevice();
    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();
    
    Result Destroy();

    const char *app_name;
    const char *engine_name;
    
    Swapchain *swapchain = nullptr;

    /* Per frame data */
    FrameHandler *frame_handler;

private:
    RefCountedPtr<Application> m_application;

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;

    DescriptorPool descriptor_pool;

    VmaAllocator allocator = nullptr;

    Device *device = nullptr;

    DeviceQueue queue_graphics,
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

