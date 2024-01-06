#ifndef RENDERER_RESULT_H
#define RENDERER_RESULT_H

#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
    
#define HYPERION_VK_CHECK(vk_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            return ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, #vk_result " != VK_SUCCESS", int(vk_result)); \
    } while (0)

#define HYPERION_VK_CHECK_MSG(vk_result, msg) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            return ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, msg, int(vk_result)); \
    } while (0)

#define HYPERION_VK_PASS_ERRORS(vk_result, out_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            (out_result) = ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, #vk_result " != VK_SUCCESS", int(vk_result)); \
    } while (0)

#define HYPERION_VK_PASS_ERRORS_MSG(vk_result, msg, out_result) \
    do { \
        if ((vk_result) != VK_SUCCESS) \
            (out_result) = ::hyperion::renderer::Result(::hyperion::renderer::Result::RENDERER_ERR, msg, int(vk_result)); \
    } while (0)

} // namespace renderer
} // namespace hyperion

#endif