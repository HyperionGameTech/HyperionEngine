/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

namespace Hyperion {
namespace utilities {

template <class FunctionType>
class DeferredScope
{
public:
    DeferredScope(FunctionType&& fn)
        : m_fn(std::move(fn))
    {
    }

    DeferredScope(const DeferredScope& other) = delete;
    DeferredScope& operator=(const DeferredScope& other) = delete;
    DeferredScope(DeferredScope&& other) noexcept = delete;
    DeferredScope& operator=(DeferredScope&& other) noexcept = delete;

    ~DeferredScope()
    {
        m_fn();
    }

private:
    FunctionType m_fn;
};

} // namespace utilities

using utilities::DeferredScope;

} // namespace Hyperion

#define HYP_DEFER(...) DeferredScope HYP_UNIQUE_NAME(scope)([&] __VA_ARGS__)
