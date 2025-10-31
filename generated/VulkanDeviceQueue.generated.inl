#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region VulkanDeviceQueueType Reflection Data

HYP_BEGIN_ENUM(VulkanDeviceQueueType, 362, 0, {})
    HypConstant(NAME(HYP_STR(GRAPHICS)), VulkanDeviceQueueType::GRAPHICS),
    HypConstant(NAME(HYP_STR(COMPUTE)), VulkanDeviceQueueType::COMPUTE),
    HypConstant(NAME(HYP_STR(TRANSFER)), VulkanDeviceQueueType::TRANSFER),
    HypConstant(NAME(HYP_STR(PRESENT)), VulkanDeviceQueueType::PRESENT)
HYP_END_ENUM

#pragma endregion VulkanDeviceQueueType Reflection Data

} // namespace hyperion

