#ifndef HYPERION_RENDERER_BACKEND_VULKAN_INSTANCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_INSTANCE_HPP

#include <vector>
#include <set>
#include <optional>
#include <array>

#include <core/Containers.hpp>

#include <vulkan/vulkan.h>

#include <system/vma/VmaUsage.hpp>

#include <rendering/backend/RenderObject.hpp>
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

class Application;

namespace renderer {
namespace platform {

template <>
class Instance<Platform::VULKAN>
{
    static ExtensionMap GetExtensionMap();

    std::vector<VkPhysicalDevice> EnumeratePhysicalDevices();
    VkPhysicalDevice PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices);

    /* Setup debug mode */
    Result SetupDebug();
    Result SetupDebugMessenger();

    Result CreateCommandPool(DeviceQueue &queue, uint index);

public:
    Instance(RC<Application> application);
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
                                                          
    VkCommandPool GetGraphicsCommandPool(uint index = 0) const { return this->queue_graphics.command_pools[index]; }
    VkCommandPool GetComputeCommandPool(uint index = 0) const { return this->queue_compute.command_pools[index]; }
    
    VkInstance GetInstance() const
        { return this->instance; }

    Swapchain<Platform::VULKAN> *GetSwapchain() const
        { return m_swapchain; }
                                                          
    FrameHandler<Platform::VULKAN> *GetFrameHandler() const
        { return frame_handler; }

    StagingBufferPool &GetStagingBufferPool()
        { return m_staging_buffer_pool; }

    const StagingBufferPool &GetStagingBufferPool() const
        { return m_staging_buffer_pool; }

    VmaAllocator GetAllocator() const
        { return allocator; }

    helpers::SingleTimeCommands GetSingleTimeCommands();

    void SetValidationLayers(Array<const char *> validation_layers);

    Device<Platform::VULKAN> *GetDevice() const
        { return m_device; }

    Result InitializeDevice(VkPhysicalDevice _physical_device = nullptr);
    Result InitializeSwapchain();
    
    Result Destroy();

    const char                      *app_name;
    const char                      *engine_name;

    /* Per frame data */
    FrameHandler<Platform::VULKAN>  *frame_handler;

private:
    RC<Application>                 m_application;

    VkInstance                      instance = nullptr;
    VkSurfaceKHR                    surface = nullptr;

    DescriptorPool                  descriptor_pool;

    VmaAllocator                    allocator = nullptr;

    Device<Platform::VULKAN>        *m_device = nullptr;
    Swapchain<Platform::VULKAN>     *m_swapchain = nullptr;

    DeviceQueue                     queue_graphics;
    DeviceQueue                     queue_transfer;
    DeviceQueue                     queue_present;
    DeviceQueue                     queue_compute;
    
    Array<const char *>             validation_layers;

    StagingBufferPool               m_staging_buffer_pool;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT        debug_messenger;
#endif
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE

