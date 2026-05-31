/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Debug/Debug.hpp>

namespace Hyperion {
#define VULKAN_CHECK(vkResult)                                                               \
    do                                                                                       \
    {                                                                                        \
        if ((vkResult) != VK_SUCCESS)                                                        \
            return HYP_MAKE_ERROR(RendererError, #vkResult " != VK_SUCCESS", int(vkResult)); \
    }                                                                                        \
    while (0)

#define VULKAN_CHECK_MSG(vkResult, msg)                               \
    do                                                                \
    {                                                                 \
        if ((vkResult) != VK_SUCCESS)                                 \
            return HYP_MAKE_ERROR(RendererError, msg, int(vkResult)); \
    }                                                                 \
    while (0)

#define VULKAN_PASS_ERRORS(vkResult, outResult)                                                     \
    do                                                                                              \
    {                                                                                               \
        if ((vkResult) != VK_SUCCESS)                                                               \
            (outResult) = HYP_MAKE_ERROR(RendererError, #vkResult " != VK_SUCCESS", int(vkResult)); \
    }                                                                                               \
    while (0)

#define VULKAN_PASS_ERRORS_MSG(vkResult, msg, outResult)                     \
    do                                                                       \
    {                                                                        \
        if ((vkResult) != VK_SUCCESS)                                        \
            (outResult) = HYP_MAKE_ERROR(RendererError, msg, int(vkResult)); \
    }                                                                        \
    while (0)

} // namespace Hyperion
