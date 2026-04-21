/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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

    HYP_EXPORT const TypeInfo* TypeInfo_GetElementTypeInfo(const TypeInfo* typeInfo)
    {
        if (!typeInfo)
        {
            return nullptr;
        }

        return typeInfo->GetElementType();
    }

    // Creates a default (zero-initialised) BoxedValue for the given TypeInfo.
    // Returns true on success; the caller owns the result and must call BoxedValue_Destruct.
    HYP_EXPORT int8 TypeInfo_CreateDefaultValue(const TypeInfo* typeInfo, BoxedValue* pOutBoxed)
    {
        if (!typeInfo || !pOutBoxed)
        {
            return false;
        }

        // For types that have a handler (vectors, enums with underlying types, etc.)
        if (typeInfo->extendedInfo.handler)
        {
            new (pOutBoxed) BoxedValue();
            return typeInfo->extendedInfo.handler->CreateInstance(*pOutBoxed) ? 1 : 0;
        }

        // For struct/class types
        if (const Class* cls = typeInfo->GetClass())
        {
            if (!cls->CanCreateInstance())
            {
                return false;
            }

            new (pOutBoxed) BoxedValue();
            return cls->CreateInstance(*pOutBoxed, /* allowAbstract */ false) ? 1 : 0;
        }

        return false;
    }
} // extern "C"
