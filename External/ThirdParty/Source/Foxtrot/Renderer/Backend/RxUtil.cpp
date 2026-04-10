#include "RxUtil.hpp"

#include "RxExtensionHandles.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

void RxUtil::SetDebugLabel_(const char* name, VkObjectType object_type, unsigned long long obj_handle)
{
    VkDebugUtilsObjectNameInfoEXT debug_info { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                               .pNext = nullptr,
                                               .objectType = object_type,
                                               .objectHandle = static_cast<uint64>(obj_handle),
                                               .pObjectName = name };

    const RxGpuDevice* device = gRenderer->GetDevice();

    Rx_EXT_SetDebugUtilsObjectName(gRenderer->GetVulkanInstance(), device->Device, &debug_info);
}
