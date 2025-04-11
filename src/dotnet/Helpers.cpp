/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Helpers.hpp>
#include <dotnet/Object.hpp>

#include <core/filesystem/FilePath.hpp>

namespace hyperion::dotnet {

namespace detail {

// void *TransformArgument<Object *>::operator()(Object *value) const
// {
//     return value->GetObjectReference().weak_handle;
// }

const char *TransformArgument<FilePath>::operator()(const FilePath &value) const
{
    return value.Data();
}

} // namespace detail

} // namespace hyperion::dotnet