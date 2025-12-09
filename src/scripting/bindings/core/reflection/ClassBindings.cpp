/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/Object.hpp>
#include <core/reflection/Method.hpp>

#include <core/logging/LogChannels.hpp>

#include <core/utilities/GlobalContext.hpp>
#include <core/reflection/TypeInfoFwd.hpp>

#include <core/Name.hpp>

#include <dotnet/DotNETHost.hpp>
#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
#include <scripting/ScriptObjectResource.hpp>
#endif

using namespace hyperion;

template <class MemberType, auto GetMemberFunc>
uint32 Class_GetMembersGeneric(const Class* cls, const MemberType** outMembers)
{
    Assert(cls != nullptr);

    Array<MemberType*> allMembers;

    const Class* curr = cls;

    while (curr != nullptr)
    {
        allMembers.Concat((curr->*GetMemberFunc)());

        curr = curr->GetParent();
    }

    if (!outMembers || allMembers.Empty())
    {
        return uint32(allMembers.Size());
    }

    for (uint32 i = 0; i < allMembers.Size(); i++)
    {
        outMembers[i] = allMembers[i];
    }

    return (uint32)allMembers.Size();
}

extern "C"
{

#pragma region Class

    HYP_EXPORT const Class* Class_GetClassByName(const char* name)
    {
        if (!name)
        {
            return nullptr;
        }

        const StringHash stringHash(name);

        return ClassRegistry::GetInstance().GetClass(stringHash);
    }

    HYP_EXPORT const Class* Class_GetClassByTypeId(const TypeId* typeId)
    {
        if (!typeId)
        {
            return nullptr;
        }

        return ClassRegistry::GetInstance().GetClass(*typeId);
    }

    HYP_EXPORT const Class* Class_GetClassForManagedClass(const dotnet::ManagedClass* managedClass)
    {
        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetClass();
    }

    HYP_EXPORT const Class* Class_GetClassByTypeHash(dotnet::Assembly* assembly, int32 typeHash)
    {
        Assert(assembly != nullptr);

        RC<dotnet::ManagedClass> managedClass = assembly->FindClassByTypeHash(typeHash);

        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetClass();
    }

    HYP_EXPORT void Class_GetName(const Class* cls, Name* outName)
    {
        if (!cls || !outName)
        {
            return;
        }

        *outName = cls->GetName();
    }

    HYP_EXPORT void Class_GetTypeId(const Class* cls, TypeId* outTypeId)
    {
        if (!cls || !outTypeId)
        {
            return;
        }

        *outTypeId = cls->GetTypeId();
    }

    HYP_EXPORT void Class_GetTypeInfo(const Class* cls, TypeInfo const** outPTypeInfo)
    {
        if (!cls || !outPTypeInfo)
        {
            return;
        }

        *outPTypeInfo = cls->GetTypeInfo();
    }

    HYP_EXPORT uint32 Class_GetSize(const Class* cls)
    {
        if (!cls)
        {
            return 0;
        }

        return uint32(cls->GetSize());
    }

    HYP_EXPORT uint32 Class_GetFlags(const Class* cls)
    {
        if (!cls)
        {
            return 0;
        }

        return uint32(cls->GetFlags());
    }

    HYP_EXPORT uint8 Class_GetAllocationMethod(const Class* cls)
    {
        if (!cls)
        {
            return uint8(ClassAllocationMethod::INVALID);
        }

        return uint8(cls->GetAllocationMethod());
    }

    HYP_EXPORT const ClassAttribute* Class_GetAttribute(const Class* cls, const char* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        auto it = cls->GetAttributes().Find(name);

        if (it == cls->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT uint32 Class_GetAttributes(const Class* cls, typename ClassAttributeSet::ConstIterator* outIter)
    {
        if (!cls || !outIter)
        {
            return 0;
        }

        if (cls->GetAttributes().Empty())
        {
            return 0;
        }

        *outIter = cls->GetAttributes().Begin();

        return (uint32)cls->GetAttributes().Size();
    }

    HYP_EXPORT const ClassAttribute* Class_NextAttribute(const Class* cls, typename ClassAttributeSet::ConstIterator* iter)
    {
        if (!cls || !iter || *iter == cls->GetAttributes().End())
        {
            return nullptr;
        }

        const ClassAttribute* attribute = &**iter;

        ++(*iter);

        return attribute;
    }

    HYP_EXPORT uint32 Class_GetProperties(const Class* cls, const Property** outProperties)
    {
        return Class_GetMembersGeneric<Property, &Class::GetProperties>(cls, outProperties);
    }

    HYP_EXPORT Property* Class_GetProperty(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetProperty(*name);
    }

    HYP_EXPORT uint32 Class_GetMethods(const Class* cls, const Method** outMethods)
    {
        return Class_GetMembersGeneric<Method, &Class::GetMethods>(cls, outMethods);
    }

    HYP_EXPORT Method* Class_GetMethod(const Class* cls, const Name* name)
    {
        // #ifndef HYP_MSVC
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetMethod(*name);
        // #endif
        //  @FIXME: Linker error in MSVC ... GetMethod() is not linking??? Weird.
        HYP_NOT_IMPLEMENTED();
    }

    HYP_EXPORT uint32 Class_GetFields(const Class* cls, const Field** outFields)
    {
        return Class_GetMembersGeneric<Field, &Class::GetFields>(cls, outFields);
    }

    HYP_EXPORT Field* Class_GetField(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetField(*name);
    }

    HYP_EXPORT uint32 Class_GetStaticFields(const Class* cls, const StaticField** outStaticFields)
    {
        return Class_GetMembersGeneric<StaticField, &Class::GetStaticFields>(cls, outStaticFields);
    }

    HYP_EXPORT StaticField* Class_GetStaticField(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetStaticField(*name);
    }

    HYP_EXPORT Class* Class_CreateDynamicClass(const TypeId* typeId, const char* name, const Class* parentClass)
    {
        Assert(typeId != nullptr);
        Assert(name != nullptr);
        Assert(parentClass != nullptr);

#ifdef HYP_DOTNET
        return new DynamicClassInstance(*typeId, CreateNameFromDynamicString(name), parentClass, nullptr, Span<const ClassAttribute>(), ClassFlags::CLASS_TYPE, Span<HypMember>());
#else
        return nullptr;
#endif
    }

    HYP_EXPORT void Class_DestroyDynamicClass(DynamicClassInstance* cls)
    {
        Assert(cls != nullptr);

        delete cls;
    }

#pragma endregion Class

} // extern "C"
