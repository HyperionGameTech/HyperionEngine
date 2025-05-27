/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <core/utilities/TypeID.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void TypeID_ForManagedType(const char* type_name, TypeID* out_type_id)
    {
        *out_type_id = TypeID::ForManagedType(type_name);
    }
} // extern "C"