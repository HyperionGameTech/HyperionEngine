/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/debug/Debug.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void TypeId_ForManagedType(const char* pTypeName, TypeId* pOutTypeId)
    {
        Assert(pOutTypeId != nullptr);
        *pOutTypeId = TypeId::ForManagedType(pTypeName);
    }
} // extern "C"
