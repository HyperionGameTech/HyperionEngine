/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_RESULT_HPP
#define HYPERION_BACKEND_RENDERER_RESULT_HPP

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {
namespace renderer {

class RendererError final : public Error
{
public:
    RendererError()
        : Error(),
          m_error_code(0)
    {
    }

    template <auto MessageString>
    RendererError(const StaticMessage& current_function, ValueWrapper<MessageString>)
        : Error(current_function, ValueWrapper<MessageString>()),
          m_error_code(0)
    {
    }

    template <auto MessageString, class... Args>
    RendererError(const StaticMessage& current_function, ValueWrapper<MessageString>, int error_code, Args&&... args)
        : Error(current_function, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_error_code(error_code)
    {
    }

    virtual ~RendererError() override = default;

    HYP_FORCE_INLINE int GetErrorCode() const
    {
        return m_error_code;
    }

private:
    int m_error_code;
};

using RendererResult = TResult<void, RendererError>;

#define HYPERION_RETURN_OK                              \
    do                                                  \
    {                                                   \
        return ::hyperion::renderer::RendererResult {}; \
    }                                                   \
    while (0)

#define HYPERION_PASS_ERRORS(result, out_result)                 \
    do                                                           \
    {                                                            \
        ::hyperion::renderer::RendererResult _result = (result); \
        if ((out_result) && !_result)                            \
            (out_result) = _result;                              \
    }                                                            \
    while (0)

#define HYPERION_BUBBLE_ERRORS(result)                           \
    do                                                           \
    {                                                            \
        ::hyperion::renderer::RendererResult _result = (result); \
        if (!_result)                                            \
            return _result;                                      \
    }                                                            \
    while (0)

#define HYPERION_IGNORE_ERRORS(result)                           \
    do                                                           \
    {                                                            \
        ::hyperion::renderer::RendererResult _result = (result); \
        (void)_result;                                           \
    }                                                            \
    while (0)

#define HYPERION_ASSERT_RESULT(result)                                                                                              \
    do                                                                                                                              \
    {                                                                                                                               \
        auto _result = (result);                                                                                                    \
        AssertThrowMsg(_result, "[Error Code: %d]  %s", _result.GetError().GetErrorCode(), _result.GetError().GetMessage().Data()); \
    }                                                                                                                               \
    while (0)

} // namespace renderer

using renderer::RendererError;
using renderer::RendererResult;

} // namespace hyperion

#if HYP_VULKAN
    #include <rendering/backend/vulkan/RendererResult.hpp>
#else
    #error Unsupported rendering backend
#endif

#endif
