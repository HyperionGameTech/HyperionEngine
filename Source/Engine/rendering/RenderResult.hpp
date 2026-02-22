/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

class RendererError final : public Error
{
public:
    RendererError()
        : Error(),
          m_errorCode(0)
    {
    }

    template <auto CurrentFunctionString, auto MessageString>
    RendererError(ValueWrapper<CurrentFunctionString>, ValueWrapper<MessageString>)
        : Error(ValueWrapper<CurrentFunctionString>(), ValueWrapper<MessageString>()),
          m_errorCode(0)
    {
    }

    template <auto CurrentFunctionString, auto MessageString, class... Args>
    RendererError(ValueWrapper<CurrentFunctionString>, ValueWrapper<MessageString>, int errorCode, Args&&... args)
        : Error(ValueWrapper<CurrentFunctionString>(), ValueWrapper<HYP_STATIC_STRING("[Code:{}] ").template Concat<MessageString>()>(), errorCode, std::forward<Args>(args)...),
          m_errorCode(errorCode)
    {
    }

    ~RendererError() = default;

    HYP_FORCE_INLINE int GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    int m_errorCode;
};

using RendererResult = TResult<void, RendererError>;

} // namespace Hyperion