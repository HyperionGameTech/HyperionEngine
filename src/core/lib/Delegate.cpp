/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/lib/Delegate.hpp>

namespace hyperion {

namespace functional {
namespace detail {
    
DelegateHandlerData::~DelegateHandlerData()
{
    if (IsValid()) {
        AssertThrow(remove_fn != nullptr);

        remove_fn(delegate, id);
    }
}

void DelegateHandlerData::Reset()
{
    id = 0;
    delegate = nullptr;
    remove_fn = nullptr;
    detach_fn = nullptr;
}

void DelegateHandlerData::Detach(DelegateHandler &&delegate_handler)
{
    if (IsValid()) {
        AssertThrow(detach_fn != nullptr);

        detach_fn(delegate, std::move(delegate_handler));
    }
}

} // namespace detail
} // namespace functional

} // namespace hyperion