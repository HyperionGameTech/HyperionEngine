/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/Name.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

#include <core/Types.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
#include <scripting/ScriptObjectResource.hpp>
#endif

using namespace hyperion;

extern "C"
{

#pragma region HypClass

    HYP_EXPORT const HypClass* HypClass_GetClassByName(const char* name)
    {
        if (!name)
        {
            return nullptr;
        }

        const WeakName weakName(name);

        return HypClassRegistry::GetInstance().GetClass(weakName);
    }

    HYP_EXPORT const HypClass* HypClass_GetClassByTypeId(const TypeId* typeId)
    {
        if (!typeId)
        {
            return nullptr;
        }

        return HypClassRegistry::GetInstance().GetClass(*typeId);
    }

    HYP_EXPORT const HypClass* HypClass_GetClassForManagedClass(const dotnet::Class* managedClass)
    {
        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetHypClass();
    }

    HYP_EXPORT const HypClass* HypClass_GetClassByTypeHash(dotnet::Assembly* assembly, int32 typeHash)
    {
        Assert(assembly != nullptr);

        RC<dotnet::Class> managedClass = assembly->FindClassByTypeHash(typeHash);

        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetHypClass();
    }

    HYP_EXPORT void HypClass_GetName(const HypClass* hypClass, Name* outName)
    {
        if (!hypClass || !outName)
        {
            return;
        }

        *outName = hypClass->GetName();
    }

    HYP_EXPORT void HypClass_GetTypeId(const HypClass* hypClass, TypeId* outTypeId)
    {
        if (!hypClass || !outTypeId)
        {
            return;
        }

        *outTypeId = hypClass->GetTypeId();
    }

    HYP_EXPORT uint32 HypClass_GetSize(const HypClass* hypClass)
    {
        if (!hypClass)
        {
            return 0;
        }

        return uint32(hypClass->GetSize());
    }

    HYP_EXPORT uint32 HypClass_GetFlags(const HypClass* hypClass)
    {
        if (!hypClass)
        {
            return 0;
        }

        return uint32(hypClass->GetFlags());
    }

    HYP_EXPORT uint8 HypClass_GetAllocationMethod(const HypClass* hypClass)
    {
        if (!hypClass)
        {
            return uint8(HypClassAllocationMethod::INVALID);
        }

        return uint8(hypClass->GetAllocationMethod());
    }

    HYP_EXPORT const HypClassAttribute* HypClass_GetAttribute(const HypClass* hypClass, const char* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        auto it = hypClass->GetAttributes().Find(name);

        if (it == hypClass->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT uint32 HypClass_GetProperties(const HypClass* hypClass, const void** outProperties)
    {
        if (!hypClass || !outProperties)
        {
            return 0;
        }

        if (hypClass->GetProperties().Empty())
        {
            return 0;
        }

        *outProperties = hypClass->GetProperties().Begin();

        return (uint32)hypClass->GetProperties().Size();
    }

    HYP_EXPORT HypProperty* HypClass_GetProperty(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetProperty(*name);
    }

    HYP_EXPORT uint32 HypClass_GetMethods(const HypClass* hypClass, const void** outMethods)
    {
        if (!hypClass || !outMethods)
        {
            return 0;
        }

        if (hypClass->GetMethods().Empty())
        {
            return 0;
        }

        *outMethods = hypClass->GetMethods().Begin();

        return (uint32)hypClass->GetMethods().Size();
    }

    HYP_EXPORT HypMethod* HypClass_GetMethod(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetMethod(*name);
    }

    HYP_EXPORT HypField* HypClass_GetField(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetField(*name);
    }

    HYP_EXPORT uint32 HypClass_GetFields(const HypClass* hypClass, const void** outFields)
    {
        if (!hypClass || !outFields)
        {
            return 0;
        }

        if (hypClass->GetFields().Empty())
        {
            return 0;
        }

        *outFields = hypClass->GetFields().Begin();

        return (uint32)hypClass->GetFields().Size();
    }

    HYP_EXPORT HypConstant* HypClass_GetConstant(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetConstant(*name);
    }

    HYP_EXPORT uint32 HypClass_GetConstants(const HypClass* hypClass, const void** outConstants)
    {
        if (!hypClass || !outConstants)
        {
            return 0;
        }

        if (hypClass->GetConstants().Empty())
        {
            return 0;
        }

        *outConstants = hypClass->GetConstants().Begin();

        return (uint32)hypClass->GetConstants().Size();
    }

    HYP_EXPORT HypClass* HypClass_CreateDynamicHypClass(const TypeId* typeId, const char* name, const HypClass* parentHypClass)
    {
        Assert(typeId != nullptr);
        Assert(name != nullptr);
        Assert(parentHypClass != nullptr);

#ifdef HYP_DOTNET
        return new DynamicHypClassInstance(*typeId, CreateNameFromDynamicString(name), parentHypClass, nullptr, Span<const HypClassAttribute>(), HypClassFlags::NONE, Span<HypMember>());
#else
        return nullptr;
#endif
    }

    HYP_EXPORT void HypClass_DestroyDynamicHypClass(DynamicHypClassInstance* hypClass)
    {
        Assert(hypClass != nullptr);

        delete hypClass;
    }

#pragma endregion HypClass

} // extern "C"
