#ifndef RENDERER_RESULT_H
#define RENDERER_RESULT_H

#include <Util.hpp>
#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {
struct Result {
    static const Result OK;

    enum {
        RENDERER_OK = 0,
        RENDERER_ERR = 1,
        RENDERER_ERR_NEEDS_REALLOCATION = 2
    } result;

    const char *message;
    int error_code = 0;
    
    Result(decltype(result) result, const char *message = "", int error_code = 0)
        : result(result), message(message), error_code(error_code) {}
    Result(const Result &other)
        : result(other.result), message(other.message), error_code(other.error_code) {}

    HYP_FORCE_INLINE
    operator bool() const { return result == RENDERER_OK; }
};

#define HYPERION_RETURN_OK \
    do { \
        return ::hyperion::renderer::Result::OK; \
    } while (0)

#define HYPERION_PASS_ERRORS(result, out_result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        if (!_result) (out_result) = _result; \
    } while (0)

#define HYPERION_BUBBLE_ERRORS(result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        if (!_result) return _result; \
    } while (0)

#define HYPERION_IGNORE_ERRORS(result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        (void)_result; \
    } while (0)

#define HYPERION_ASSERT_RESULT(result) \
    do { \
        auto _result = (result); \
        AssertThrowMsg(_result == ::hyperion::renderer::Result::OK, "[Error Code: %d]  %s", _result.error_code, _result.message); \
    } while (0)

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