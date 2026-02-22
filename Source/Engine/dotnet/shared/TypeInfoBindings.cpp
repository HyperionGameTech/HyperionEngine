/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT int8 TypeInfo_IsValid(const TypeInfo* typeInfo)
    {
        return typeInfo->IsValid();
    }

    HYP_EXPORT void TypeInfo_GetName(const TypeInfo* typeInfo, Name* outName)
    {
        *outName = typeInfo->name;
    }

    HYP_EXPORT uint32 TypeInfo_GetSize(const TypeInfo* typeInfo)
    {
        return typeInfo->size;
    }

    HYP_EXPORT uint32 TypeInfo_GetAlignment(const TypeInfo* typeInfo)
    {
        return typeInfo->alignment;
    }

    HYP_EXPORT uint32 TypeInfo_GetFlags(const TypeInfo* typeInfo)
    {
        return typeInfo->flags;
    }

    HYP_EXPORT const Class* TypeInfo_GetClass(const TypeInfo* typeInfo)
    {
        return typeInfo->GetClass();
    }
} // extern "C"
