/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <rendering/vulkan/VulkanDevice.hpp>

#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

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

    RendererResult Initialize(bool enableDebugLayers);

    HYP_FORCE_INLINE VkInstance GetInstance() const
    {
        return m_instance;
    }

    HYP_FORCE_INLINE const VulkanDeviceRef& GetDevice() const
    {
        return m_device;
    }

    HYP_FORCE_INLINE VmaAllocator GetAllocator() const
    {
        return m_allocator;
    }

    const char* appName;
    const char* engineName;

private:
    RendererResult CreateDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

    VkInstance m_instance;

    VmaAllocator m_allocator;

    VulkanDeviceRef m_device;

#ifdef HYP_DEBUG_MODE
    Array<const char*, VulkanAllocator> m_validationLayers;
    VkDebugUtilsMessengerEXT m_vkDebugMessenger;
#endif
};

} // namespace Hyperion
