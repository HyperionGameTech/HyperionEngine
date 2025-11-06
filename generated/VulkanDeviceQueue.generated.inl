#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region VulkanDeviceQueueType Reflection Data

HYP_BEGIN_ENUM(VulkanDeviceQueueType, 349, 0, {})
    StaticField(NAME(HYP_STR(GRAPHICS)), VulkanDeviceQueueType::GRAPHICS),
    StaticField(NAME(HYP_STR(COMPUTE)), VulkanDeviceQueueType::COMPUTE),
    StaticField(NAME(HYP_STR(TRANSFER)), VulkanDeviceQueueType::TRANSFER),
    StaticField(NAME(HYP_STR(PRESENT)), VulkanDeviceQueueType::PRESENT)
HYP_END_ENUM

#pragma endregion VulkanDeviceQueueType Reflection Data

} // namespace hyperion

