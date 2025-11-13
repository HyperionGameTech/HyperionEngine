/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <rendering/vulkan/VulkanDevice.hpp>

#include <rendering/RenderObject.hpp>

#include <system/vma/VmaUsage.hpp>

#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanInstance
{
    static ExtensionMap GetExtensionMap();

#ifdef HYP_DEBUG_MODE
    RendererResult SetupDebug();
    RendererResult SetupDebugMessenger();
#endif

public:
    VulkanInstance();
    ~VulkanInstance();

    RendererResult Initialize(bool enableDebug);

    HYP_FORCE_INLINE VkInstance GetInstance() const
    {
        return m_instance;
    }

    HYP_FORCE_INLINE const VulkanDeviceRef& GetDevice() const
    {
        return m_device;
    }

    HYP_FORCE_INLINE const VulkanSwapchainRef& GetSwapchain() const
    {
        return m_swapchain;
    }

    HYP_FORCE_INLINE VmaAllocator GetAllocator() const
    {
        return allocator;
    }

    RendererResult CreateDevice(VkPhysicalDevice _physical_device = nullptr);
    RendererResult CreateSwapchain();
    RendererResult RecreateSwapchain();

    const char* appName;
    const char* engineName;

private:
    void CreateSurface();

    VkInstance m_instance;
    VkSurfaceKHR m_surface;

    VmaAllocator allocator = nullptr;

    VulkanDeviceRef m_device;
    VulkanSwapchainRef m_swapchain;

#ifdef HYP_DEBUG_MODE
    Array<const char*> m_validationLayers;
    VkDebugUtilsMessengerEXT m_vkDebugMessenger;
#endif
};

} // namespace hyperion
