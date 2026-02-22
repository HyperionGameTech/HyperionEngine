/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/threading/Mutex.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/Pair.hpp>

#include <core/logging/Logger.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/ManagedMethod.hpp>

#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/interop/ManagedAttribute.hpp>

#include <dotnet/DotNETHost.hpp>

using namespace Hyperion;
using namespace Hyperion::dotnet;

namespace Hyperion {
HYP_DECLARE_LOG_CHANNEL(DotNET);
} // namespace Hyperion

static ManagedAttributeSet InternManagedAttributeHolder(ManagedAttributeHolder* managedAttributeHolderPtr)
{
    if (!managedAttributeHolderPtr)
    {
        return {};
    }

    Array<UniquePtr<ManagedObject>> attributes;
    attributes.Reserve(managedAttributeHolderPtr->managedAttributesSize);

    for (uint32 i = 0; i < managedAttributeHolderPtr->managedAttributesSize; i++)
    {
        Assert(managedAttributeHolderPtr->managedAttributesPtr[i].classPtr != nullptr);

        attributes.PushBack(MakeUnique<ManagedObject>(
            managedAttributeHolderPtr->managedAttributesPtr[i].classPtr->RefCountedPtrFromThis(),
            managedAttributeHolderPtr->managedAttributesPtr[i].objectReference,
            ObjectFlags::CREATED_FROM_MANAGED));
    }

    return ManagedAttributeSet(std::move(attributes));
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

    HYP_EXPORT void NativeInterop_SetInvokeGetterFunction(ManagedGuid* assemblyGuid, Assembly* pAssembly, InvokeGetterFunction invokeGetterFptr)
    {
        Assert(pAssembly != nullptr);

        pAssembly->SetInvokeGetterFunction(invokeGetterFptr);
    }

    HYP_EXPORT void NativeInterop_SetInvokeSetterFunction(ManagedGuid* assemblyGuid, Assembly* pAssembly, InvokeSetterFunction invokeSetterFptr)
    {
        Assert(pAssembly != nullptr);

        pAssembly->SetInvokeSetterFunction(invokeSetterFptr);
    }

    HYP_EXPORT void NativeInterop_SetAddObjectToCacheFunction(AddObjectToCacheFunction addObjectToCacheFptr)
    {
        DotNETHost::GetInstance().GetGlobalFunctions().addObjectToCacheFunction = addObjectToCacheFptr;
    }

    HYP_EXPORT void NativeInterop_SetSetKeepAliveFunction(SetKeepAliveFunction setKeepAliveFunction)
    {
        DotNETHost::GetInstance().GetGlobalFunctions().setKeepAliveFunction = setKeepAliveFunction;
    }

    HYP_EXPORT void NativeInterop_SetTriggerGCFunction(TriggerGCFunction triggerGcFunction)
    {
        DotNETHost::GetInstance().GetGlobalFunctions().triggerGcFunction = triggerGcFunction;
    }

    HYP_EXPORT void NativeInterop_SetGetAssemblyPointerFunction(GetAssemblyPointerFunction getAssemblyPointerFunction)
    {
        DotNETHost::GetInstance().GetGlobalFunctions().getAssemblyPointerFunction = getAssemblyPointerFunction;
    }

    HYP_EXPORT void NativeInterop_SetCleanupOnShutdownFunction(CleanupOnShutdownFunction cleanupOnShutdownFunction)
    {
        DotNETHost::GetInstance().GetGlobalFunctions().cleanupOnShutdownFunction = cleanupOnShutdownFunction;
    }

    HYP_EXPORT void NativeInterop_GetAssemblyPointer(ObjectReference* assemblyObjectReference, Assembly** pOutAssembly)
    {
        Assert(assemblyObjectReference != nullptr);
        Assert(pOutAssembly != nullptr);

        *pOutAssembly = nullptr;

        DotNETHost::GetInstance().GetGlobalFunctions().getAssemblyPointerFunction(assemblyObjectReference, pOutAssembly);

        AssertDebug(*pOutAssembly != nullptr);
    }

    HYP_EXPORT void NativeInterop_AddObjectToCache(void* ptr, ManagedClass** outClass, ObjectReference* outObjectReference, int8 weak)
    {
        Assert(ptr != nullptr);
        Assert(outClass != nullptr);
        Assert(outObjectReference != nullptr);

        DotNETHost::GetInstance().GetGlobalFunctions().addObjectToCacheFunction(ptr, outClass, outObjectReference, weak);
    }

    HYP_EXPORT int NativeInterop_NewAssembly(ManagedGuid guid, Assembly** pOutAssembly)
    {
        Assert(pOutAssembly != nullptr);

        *pOutAssembly = nullptr;

        RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(guid);
        *pOutAssembly = assembly.Get();
        memory::detail::IncStrong(assembly.GetBlock_Internal()); // keep alive after this function returns

        return int(LoadAssemblyResult::OK);
    }

    HYP_EXPORT void NativeInterop_FreeAssembly(Assembly* pAssembly)
    {
        Assert(pAssembly != nullptr);

        Weak w = pAssembly->WeakRefCountedPtrFromThis();
        memory::detail::ReleaseStrong(w.GetBlock_Internal()); // release the strong reference created by NewAssembly
    }

    HYP_EXPORT void ManagedClass_Create(ManagedGuid* assemblyGuid, Assembly* pAssembly, const Class* cls, int32 typeHash, const char* typeName, uint32 typeSize, TypeId typeId, ManagedClass* parentClass, uint32 flags, ManagedClassDesc* outDesc)
    {
#ifdef HYP_DOTNET
        Assert(assemblyGuid != nullptr);
        Assert(pAssembly != nullptr);

        HYP_LOG(DotNET, Debug, "Registering .NET managed class {} for Assembly {}", typeName, pAssembly->GetGuid());

        RC<ManagedClass> classObject = pAssembly->NewClass(cls, typeHash, typeName, typeSize, typeId, parentClass, flags);

        if (cls != nullptr && cls->IsDynamic())
        {
            const DynamicClassInstance* dynamicClass = static_cast<const DynamicClassInstance*>(cls);

            if ((classObject->GetFlags() & ManagedClassFlags::ABSTRACT) && !dynamicClass->IsAbstract())
            {
                HYP_LOG(DotNET, Error, "Dynamic Class {} is not abstract but the managed class {} is abstract!",
                    dynamicClass->GetName(), classObject->GetName());
            }

            DynamicClassInstance* dynamicClassNonConst = const_cast<DynamicClassInstance*>(dynamicClass);
            dynamicClassNonConst->SetManagedClass(classObject);

            /// \todo Implement unregistering of dynamic hyp classes
            ClassRegistry::GetInstance().RegisterClass(typeId, dynamicClassNonConst);
        }

        ManagedClassDesc& desc = *outDesc;
        desc = {};
        desc.typeHash = typeHash;
        desc.pClass = classObject.Get();
        desc.assemblyGuid = *assemblyGuid;
        desc.flags = flags;
#endif
    }

    HYP_EXPORT int8 ManagedClass_FindByTypeHash(Assembly* pAssembly, int32 typeHash, ManagedClass** outClass)
    {
        Assert(pAssembly != nullptr);

        Assert(outClass != nullptr);

        RC<ManagedClass> classObject = pAssembly->FindClassByTypeHash(typeHash);

        if (!classObject)
        {
            *outClass = nullptr;

            return 0;
        }

        *outClass = classObject;

        return 1;
    }

    HYP_EXPORT void ManagedClass_SetAttributes(ManagedClass* managedClass, ManagedAttributeHolder* managedAttributeHolderPtr)
    {
        Assert(managedClass != nullptr);

        if (!managedAttributeHolderPtr)
        {
            return;
        }

        HYP_LOG(DotNET, Debug, "Setting attributes for managed class '{}'", managedClass->GetName());

        ManagedAttributeSet attributes = InternManagedAttributeHolder(managedAttributeHolderPtr);

        managedClass->SetAttributes(std::move(attributes));
    }

    HYP_EXPORT void ManagedClass_AddMethod(ManagedClass* managedClass, const char* methodName, ManagedGuid guid, InvokeMethodFunction invokeFptr, ManagedAttributeHolder* managedAttributeHolderPtr)
    {
        Assert(managedClass != nullptr);
        Assert(invokeFptr != nullptr);

        if (!methodName)
        {
            return;
        }

        ManagedAttributeSet attributes = InternManagedAttributeHolder(managedAttributeHolderPtr);

        if (managedClass->HasMethod(methodName))
        {
            HYP_LOG(DotNET, Error, "Class '{}' already has a method named '{}'!", managedClass->GetName(), methodName);

            return;
        }

        managedClass->AddMethod(
            methodName,
            ManagedMethod(guid, invokeFptr, std::move(attributes)));
    }

    HYP_EXPORT void ManagedClass_AddProperty(ManagedClass* managedClass, const char* propertyName, ManagedGuid guid, ManagedAttributeHolder* managedAttributeHolderPtr)
    {
        Assert(managedClass != nullptr);

        if (!propertyName)
        {
            return;
        }

        ManagedAttributeSet attributes = InternManagedAttributeHolder(managedAttributeHolderPtr);

        if (managedClass->HasProperty(propertyName))
        {
            HYP_LOG(DotNET, Error, "Class '{}' already has a property named '{}'!", managedClass->GetName(), propertyName);

            return;
        }

        managedClass->AddProperty(
            propertyName,
            ManagedProperty(guid, std::move(attributes)));
    }

    HYP_EXPORT void ManagedClass_SetNewObjectFunction(ManagedClass* managedClass, ManagedClass::NewObjectFunction newObjectFptr)
    {
        Assert(managedClass != nullptr);

        managedClass->SetNewObjectFunction(newObjectFptr);
    }

    HYP_EXPORT void ManagedClass_SetMarshalObjectFunction(ManagedClass* managedClass, ManagedClass::MarshalObjectFunction marshalObjectFptr)
    {
        Assert(managedClass != nullptr);

        managedClass->SetMarshalObjectFunction(marshalObjectFptr);
    }

} // extern "C"
