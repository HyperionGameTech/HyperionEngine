/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_RESULT_H
#define HYPERION_V2_BACKEND_RENDERER_RESULT_H

#include <core/Defines.hpp>
#include <system/Debug.hpp>

namespace hyperion {
namespace renderer {

struct Result
{
    enum
    {
        RENDERER_OK                     = 0,
        RENDERER_ERR                    = 1,
        RENDERER_ERR_NEEDS_REALLOCATION = 2
    } result;

    const char  *message;
    int         error_code = 0;

    Result()
        : result(RENDERER_OK), message(""), error_code(0)
    {
    }
    
    Result(decltype(result) result, const char *message = "", int error_code = 0)
        : result(result), message(message), error_code(error_code)
    {
    }

    Result(const Result &other)
        : result(other.result), message(other.message), error_code(other.error_code)
    {
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static Result OK()
        { return Result(RENDERER_OK); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    operator bool() const
        { return result == RENDERER_OK; }
};

#define HYPERION_RETURN_OK \
    do { \
        return ::hyperion::renderer::Result { }; \
    } while (0)

#define HYPERION_PASS_ERRORS(result, out_result) \
    do { \
        ::hyperion::renderer::Result _result = (result); \
        if ((out_result) && !_result) (out_result) = _result; \
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
        AssertThrowMsg(_result == ::hyperion::renderer::Result { }, "[Error Code: %d]  %s", _result.error_code, _result.message); \
    } while (0)

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererResult.hpp>
#else
#error Unsupported rendering backend
#endif

#endif