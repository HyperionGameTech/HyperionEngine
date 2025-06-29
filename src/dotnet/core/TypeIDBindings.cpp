/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <core/utilities/TypeId.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void TypeId_ForManagedType(const char* type_name, TypeId* out_type_id)
    {
        *out_type_id = TypeId::ForManagedType(type_name);
    }
} // extern "C"