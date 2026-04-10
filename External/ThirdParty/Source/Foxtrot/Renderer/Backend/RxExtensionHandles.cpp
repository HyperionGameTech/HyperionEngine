#include "RxExtensionHandles.hpp"

#include <Core/FxLog.hpp>

VkResult Rx_EXT_SetDebugUtilsObjectName(VkInstance instance, VkDevice device,
                                        const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
    using TFn = VkResult (*)(VkDevice, const VkDebugUtilsObjectNameInfoEXT*);
    const auto ext_function = RxGetExtensionFunc<TFn>(instance, "vkSetDebugUtilsObjectNameEXT");
    return ext_function(device, pNameInfo);
}


VkResult Rx_EXT_CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    using TFn = VkResult (*)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*,
                             VkDebugUtilsMessengerEXT*);

    const auto ext_function = RxGetExtensionFunc<TFn>(instance, "vkCreateDebugUtilsMessengerEXT");
    return ext_function(instance, pCreateInfo, pAllocator, pDebugMessenger);
}


void Rx_EXT_DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                       const VkAllocationCallbacks* pAllocator)
{
    using TFn = void (*)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);

    const auto ext_function = RxGetExtensionFunc<TFn>(instance, "vkDestroyDebugUtilsMessengerEXT");
    return ext_function(instance, messenger, pAllocator);
}
