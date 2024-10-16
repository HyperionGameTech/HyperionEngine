/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Helpers.hpp>
#include <dotnet/Object.hpp>

namespace hyperion::dotnet {

namespace detail {

void *TransformArgument<Object *>::operator()(Object *value) const
{
    return value->GetObjectReference().ptr;
}

} // namespace detail

} // namespace hyperion::dotnet