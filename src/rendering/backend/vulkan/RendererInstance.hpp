/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_INSTANCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_INSTANCE_HPP

#include <core/Defines.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <system/vma/VmaUsage.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

namespace sys {
class AppContext;
} // namespace sys

using sys::AppContext;

namespace renderer {
namespace platform {

template <>
class Instance<Platform::VULKAN>
{
    static ExtensionMap GetExtensionMap();

    /* Setup debug mode */
    RendererResult SetupDebug();
    RendererResult SetupDebugMessenger();

public:
    Instance();
    RendererResult Initialize(const AppContext &app_context, bool load_debug_layers = false);

    HYP_FORCE_INLINE VkInstance GetInstance() const
        { return m_instance; }

    HYP_FORCE_INLINE Device<Platform::VULKAN> *GetDevice() const
        { return m_device; }

    HYP_FORCE_INLINE const SwapchainRef<Platform::VULKAN> &GetSwapchain() const
        { return m_swapchain; }

    HYP_FORCE_INLINE VmaAllocator GetAllocator() const
        { return allocator; }

    void SetValidationLayers(Array<const char *> validation_layers);

    RendererResult CreateDevice(VkPhysicalDevice _physical_device = nullptr);
    RendererResult CreateSwapchain();
    RendererResult RecreateSwapchain();
    
    RendererResult Destroy();

    const char                      *app_name;
    const char                      *engine_name;

private:
    void CreateSurface();

    VkInstance                      m_instance;
    VkSurfaceKHR                    m_surface;

    VmaAllocator                    allocator = nullptr;

    Device<Platform::VULKAN>        *m_device = nullptr;
    SwapchainRef<Platform::VULKAN>  m_swapchain;
    
    Array<const char *>             validation_layers;

#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerEXT        debug_messenger;
#endif
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //!RENDERER_INSTANCE

