/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/Result.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

class RendererError final : public Error
{
public:
    RendererError()
        : Error(),
          m_errorCode(0)
    {
    }

    template <auto MessageString>
    RendererError(const StaticMessage& currentFunction, ValueWrapper<MessageString>)
        : Error(currentFunction, ValueWrapper<MessageString>()),
          m_errorCode(0)
    {
    }

    template <auto MessageString, class... Args>
    RendererError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, int errorCode, Args&&... args)
        : Error(currentFunction, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_errorCode(errorCode)
    {
    }

    virtual ~RendererError() override = default;

    HYP_FORCE_INLINE int GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    int m_errorCode;
};

using RendererResult = TResult<void, RendererError>;

#define HYPERION_RETURN_OK                    \
    do                                        \
    {                                         \
        return ::hyperion::RendererResult {}; \
    }                                         \
    while (0)

#define HYPERION_PASS_ERRORS(result, outResult)        \
    do                                                 \
    {                                                  \
        ::hyperion::RendererResult _result = (result); \
        if ((outResult) && !_result)                   \
            (outResult) = _result;                     \
    }                                                  \
    while (0)

/// On error, exits the current functon returning the result
#define HYP_GFX_CHECK(result)                          \
    do                                                 \
    {                                                  \
        ::hyperion::RendererResult _result = (result); \
        if (!_result)                                  \
            return _result;                            \
    }                                                  \
    while (0)

#define HYP_GFX_ASSERT(cond, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (HYP_UNLIKELY(!(cond)))                                                                 \
        {                                                                                          \
            std::printf(                                                                           \
                "Assertion failed in renderer!\n\tCondition: " #cond "\n\tMessage: " __VA_ARGS__); \
            HYP_PRINT_STACK_TRACE();                                                               \
            std::terminate();                                                                      \
        }                                                                                          \
    }                                                                                              \
    while (0)

#define HYPERION_IGNORE_ERRORS(result)                 \
    do                                                 \
    {                                                  \
        ::hyperion::RendererResult _result = (result); \
        (void)_result;                                 \
    }                                                  \
    while (0)

} // namespace hyperion

#if HYP_VULKAN
#include <rendering/vulkan/VulkanResult.hpp>
#else
#error Unsupported rendering backend
#endif
