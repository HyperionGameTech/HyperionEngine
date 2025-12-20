/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/debug/Debug.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void TypeId_ForManagedType(const char* pTypeName, TypeId* pOutTypeId)
    {
        Assert(pOutTypeId != nullptr);
        *pOutTypeId = TypeId::ForManagedType(pTypeName);
    }
} // extern "C"
