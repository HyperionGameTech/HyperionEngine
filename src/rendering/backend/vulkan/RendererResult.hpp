/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_RESULT_HPP
#define RENDERER_RESULT_HPP

#include <core/debug/Debug.hpp>

namespace hyperion {
namespace renderer {
    
#define HYPERION_VK_CHECK(vk_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            return HYP_MAKE_ERROR(RendererError, #vk_result " != VK_SUCCESS", int(vk_result)); \
    } while (0)

#define HYPERION_VK_CHECK_MSG(vk_result, msg) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            return HYP_MAKE_ERROR(RendererError, msg, int(vk_result)); \
    } while (0)

#define HYPERION_VK_PASS_ERRORS(vk_result, out_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            (out_result) = HYP_MAKE_ERROR(RendererError, #vk_result " != VK_SUCCESS", int(vk_result)); \
    } while (0)

#define HYPERION_VK_PASS_ERRORS_MSG(vk_result, msg, out_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            (out_result) = HYP_MAKE_ERROR(RendererError, msg, int(vk_result)); \
    } while (0)

} // namespace renderer
} // namespace hyperion

#endif