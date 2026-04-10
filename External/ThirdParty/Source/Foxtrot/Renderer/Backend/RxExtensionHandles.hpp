#pragma once

#include <vulkan/vulkan.h>

#include <Core/FxLog.hpp>

using RxVoidFunction = void (*)(void);

template <typename TFunc>
TFunc RxGetExtensionFunc(VkInstance instance, const char* name)
{
    RxVoidFunction handle = vkGetInstanceProcAddr(instance, name);

    if (!handle) {
        FxLogError("Could not load extension function '{}'", name);
    }

    return reinterpret_cast<TFunc>(handle);
}

VkResult Rx_EXT_SetDebugUtilsObjectName(VkInstance instance, VkDevice device,
                                        const VkDebugUtilsObjectNameInfoEXT* pNameInfo);

VkResult Rx_EXT_CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);

void Rx_EXT_DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                       const VkAllocationCallbacks* pAllocator);
