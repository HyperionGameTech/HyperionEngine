/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/debug/Debug.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void TypeId_ForManagedType(const char* typeName, TypeId* outTypeId)
    {
        *outTypeId = TypeId::ForManagedType(typeName);
    }
} // extern "C"
