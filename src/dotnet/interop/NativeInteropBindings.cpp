/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/Pair.hpp>

#include <core/logging/Logger.hpp>

#include <core/reflection/HypClass.hpp>
#include <core/reflection/HypClassRegistry.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Method.hpp>

#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/interop/ManagedObject.hpp>
#include <dotnet/interop/ManagedAttribute.hpp>

#include <dotnet/DotNetSystem.hpp>

using namespace hyperion;
using namespace hyperion::dotnet;

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(DotNET);
} // namespace hyperion

static AttributeSet InternManagedAttributeHolder(ManagedAttributeHolder* managedAttributeHolderPtr)
{
    if (!managedAttributeHolderPtr)
    {
        return {};
    }

    Array<Attribute> attributes;
    attributes.Reserve(managedAttributeHolderPtr->managedAttributesSize);

    for (uint32 i = 0; i < managedAttributeHolderPtr->managedAttributesSize; i++)
    {
        Assert(managedAttributeHolderPtr->managedAttributesPtr[i].classPtr != nullptr);

        attributes.PushBack(Attribute {
            MakeUnique<Object>(
                managedAttributeHolderPtr->managedAttributesPtr[i].classPtr->RefCountedPtrFromThis(),
                managedAttributeHolderPtr->managedAttributesPtr[i].objectReference,
                ObjectFlags::CREATED_FROM_MANAGED) });
    }

    return AttributeSet(std::move(attributes));
}

extern "C"
{
    HYP_EXPORT bool NativeInterop_VerifyEngineVersion(uint32 assemblyEngineVersion, bool major, bool minor, bool patch)
    {
        static constexpr uint32 majorMask = (0xffu << 16u);
        static constexpr uint32 minorMask = (0xffu << 8u);
        static constexpr uint32 patchMask = 0xffu;

        const uint32 mask = (major ? majorMask : 0u)
            | (minor ? minorMask : 0u)
            | (patch ? patchMask : 0u);

        const uint32 engineVersionMajorMinor = EngineVersion & mask;

        if ((assemblyEngineVersion & mask) != engineVersionMajorMinor)
        {
            HYP_LOG(DotNET, Error, "Assembly engine version mismatch: Assembly version: {}.{}.{}, Engine version: {}.{}.{}",
                (assemblyEngineVersion >> 16u) & 0xffu,
                (assemblyEngineVersion >> 8u) & 0xffu,
                assemblyEngineVersion & 0xffu,
                (EngineVersion >> 16u) & 0xffu,
                (EngineVersion >> 8u) & 0xffu,
                EngineVersion & 0xffu);

            return false;
        }

        return true;
    }

    HYP_EXPORT void NativeInterop_SetInvokeGetterFunction(ManagedGuid* assemblyGuid, Assembly* assemblyPtr, InvokeGetterFunction invokeGetterFptr)
    {
        Assert(assemblyPtr != nullptr);

        assemblyPtr->SetInvokeGetterFunction(invokeGetterFptr);
    }

    HYP_EXPORT void NativeInterop_SetInvokeSetterFunction(ManagedGuid* assemblyGuid, Assembly* assemblyPtr, InvokeSetterFunction invokeSetterFptr)
    {
        Assert(assemblyPtr != nullptr);

        assemblyPtr->SetInvokeSetterFunction(invokeSetterFptr);
    }

    HYP_EXPORT void NativeInterop_SetAddObjectToCacheFunction(AddObjectToCacheFunction addObjectToCacheFptr)
    {
        DotNetSystem::GetInstance().GetGlobalFunctions().addObjectToCacheFunction = addObjectToCacheFptr;
    }

    HYP_EXPORT void NativeInterop_SetSetKeepAliveFunction(SetKeepAliveFunction setKeepAliveFunction)
    {
        DotNetSystem::GetInstance().GetGlobalFunctions().setKeepAliveFunction = setKeepAliveFunction;
    }

    HYP_EXPORT void NativeInterop_SetTriggerGCFunction(TriggerGCFunction triggerGcFunction)
    {
        DotNetSystem::GetInstance().GetGlobalFunctions().triggerGcFunction = triggerGcFunction;
    }

    HYP_EXPORT void NativeInterop_SetGetAssemblyPointerFunction(GetAssemblyPointerFunction getAssemblyPointerFunction)
    {
        DotNetSystem::GetInstance().GetGlobalFunctions().getAssemblyPointerFunction = getAssemblyPointerFunction;
    }

    HYP_EXPORT void NativeInterop_GetAssemblyPointer(ObjectReference* assemblyObjectReference, Assembly** outAssemblyPtr)
    {
        Assert(assemblyObjectReference != nullptr);
        Assert(outAssemblyPtr != nullptr);

        *outAssemblyPtr = nullptr;

        DotNetSystem::GetInstance().GetGlobalFunctions().getAssemblyPointerFunction(assemblyObjectReference, outAssemblyPtr);
    }

    HYP_EXPORT void NativeInterop_AddObjectToCache(void* ptr, Class** outClassObjectPtr, ObjectReference* outObjectReference, int8 weak)
    {
        Assert(ptr != nullptr);
        Assert(outClassObjectPtr != nullptr);
        Assert(outObjectReference != nullptr);

        DotNetSystem::GetInstance().GetGlobalFunctions().addObjectToCacheFunction(ptr, outClassObjectPtr, outObjectReference, weak);
    }

    HYP_EXPORT void ManagedClass_Create(ManagedGuid* assemblyGuid, Assembly* assemblyPtr, const HypClass* hypClass, int32 typeHash, const char* typeName, uint32 typeSize, TypeId typeId, Class* parentClass, uint32 flags, ManagedClass* outManagedClass)
    {
#ifdef HYP_DOTNET
        Assert(assemblyGuid != nullptr);
        Assert(assemblyPtr != nullptr);

        HYP_LOG(DotNET, Info, "Registering .NET managed class {}", typeName);

        RC<Class> classObject = assemblyPtr->NewClass(hypClass, typeHash, typeName, typeSize, typeId, parentClass, flags);

        if (hypClass != nullptr && hypClass->IsDynamic())
        {
            const DynamicHypClassInstance* dynamicHypClass = static_cast<const DynamicHypClassInstance*>(hypClass);

            if ((classObject->GetFlags() & ManagedClassFlags::ABSTRACT) && !dynamicHypClass->IsAbstract())
            {
                HYP_LOG(DotNET, Error, "Dynamic HypClass {} is not abstract but the managed class {} is abstract!",
                    dynamicHypClass->GetName(), classObject->GetName());
            }

            DynamicHypClassInstance* dynamicHypClassNonConst = const_cast<DynamicHypClassInstance*>(dynamicHypClass);
            dynamicHypClassNonConst->SetManagedClass(classObject);

            // @TODO Implement unregistering of dynamic hyp classes
            HypClassRegistry::GetInstance().RegisterClass(typeId, dynamicHypClassNonConst);
        }

        ManagedClass& managedClass = *outManagedClass;
        managedClass = {};
        managedClass.typeHash = typeHash;
        managedClass.classObject = classObject.Get();
        managedClass.assemblyGuid = *assemblyGuid;
        managedClass.flags = flags;
#endif
    }

    HYP_EXPORT int8 ManagedClass_FindByTypeHash(Assembly* assemblyPtr, int32 typeHash, Class** outManagedClassObjectPtr)
    {
        Assert(assemblyPtr != nullptr);

        Assert(outManagedClassObjectPtr != nullptr);

        RC<Class> classObject = assemblyPtr->FindClassByTypeHash(typeHash);

        if (!classObject)
        {
            *outManagedClassObjectPtr = nullptr;

            return 0;
        }

        *outManagedClassObjectPtr = classObject;

        return 1;
    }

    HYP_EXPORT void ManagedClass_SetAttributes(ManagedClass* managedClass, ManagedAttributeHolder* managedAttributeHolderPtr)
    {
        Assert(managedClass != nullptr);

        if (!managedClass->classObject || !managedAttributeHolderPtr)
        {
            return;
        }

        AttributeSet attributes = InternManagedAttributeHolder(managedAttributeHolderPtr);

        managedClass->classObject->SetAttributes(std::move(attributes));
    }

    HYP_EXPORT void ManagedClass_AddMethod(ManagedClass* managedClass, const char* methodName, ManagedGuid guid, InvokeMethodFunction invokeFptr, ManagedAttributeHolder* managedAttributeHolderPtr)
    {
        Assert(managedClass != nullptr);
        Assert(invokeFptr != nullptr);

        if (!managedClass->classObject || !methodName)
        {
            return;
        }

        AttributeSet attributes = InternManagedAttributeHolder(managedAttributeHolderPtr);

        if (managedClass->classObject->HasMethod(methodName))
        {
            HYP_LOG(DotNET, Error, "Class '{}' already has a method named '{}'!", managedClass->classObject->GetName(), methodName);

            return;
        }

        managedClass->classObject->AddMethod(
            methodName,
            Method(guid, invokeFptr, std::move(attributes)));
    }

    HYP_EXPORT void ManagedClass_AddProperty(ManagedClass* managedClass, const char* propertyName, ManagedGuid guid, ManagedAttributeHolder* managedAttributeHolderPtr)
    {
        Assert(managedClass != nullptr);

        if (!managedClass->classObject || !propertyName)
        {
            return;
        }

        AttributeSet attributes = InternManagedAttributeHolder(managedAttributeHolderPtr);

        if (managedClass->classObject->HasProperty(propertyName))
        {
            HYP_LOG(DotNET, Error, "Class '{}' already has a property named '{}'!", managedClass->classObject->GetName(), propertyName);

            return;
        }

        managedClass->classObject->AddProperty(
            propertyName,
            Property(guid, std::move(attributes)));
    }

    HYP_EXPORT void ManagedClass_SetNewObjectFunction(ManagedClass* managedClass, Class::NewObjectFunction newObjectFptr)
    {
        Assert(managedClass != nullptr);
        Assert(managedClass->classObject != nullptr);

        managedClass->classObject->SetNewObjectFunction(newObjectFptr);
    }

    HYP_EXPORT void ManagedClass_SetMarshalObjectFunction(ManagedClass* managedClass, Class::MarshalObjectFunction marshalObjectFptr)
    {
        Assert(managedClass != nullptr);
        Assert(managedClass->classObject != nullptr);

        managedClass->classObject->SetMarshalObjectFunction(marshalObjectFptr);
    }

} // extern "C"
