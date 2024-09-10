/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_INTEROP_MANAGED_METHOD_HPP
#define HYPERION_DOTNET_INTEROP_MANAGED_METHOD_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion::dotnet {

struct ManagedMethod
{
    ManagedGuid     guid;
    Array<String>   attribute_names;

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView attribute_name) const
    {
        for (const String &name : attribute_names) {
            if (name == attribute_name) {
                return true;
            }
        }

        return false;
    }
};

} // namespace hyperion::dotnet

#endif