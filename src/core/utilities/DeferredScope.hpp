/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEFERRED_SCOPE_HPP
#define HYPERION_DEFERRED_SCOPE_HPP

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {
namespace utilities {

template <class FunctionType>
class DeferredScope
{
public:
    DeferredScope(FunctionType &&fn)
        : m_fn(std::move(fn))
    {
    }

    DeferredScope(const DeferredScope &other)                   = delete;
    DeferredScope &operator=(const DeferredScope &other)        = delete;
    DeferredScope(DeferredScope &&other) noexcept               = delete;
    DeferredScope &operator=(DeferredScope &&other) noexcept    = delete;

    ~DeferredScope()
    {
        m_fn();
    }

private:
    FunctionType    m_fn;
};

} // namespace utilities

using utilities::DeferredScope;

} // namespace hyperion

#define HYP_DEFER(...) DeferredScope HYP_UNIQUE_NAME(scope)([&] __VA_ARGS__)

#endif