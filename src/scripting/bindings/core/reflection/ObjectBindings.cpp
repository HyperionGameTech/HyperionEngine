/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/ClassRegistry.hpp>
#include <core/reflection/HypObject.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/reflection/HypObjectPool.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#include <core/Types.hpp>

#ifdef HYP_DOTNET
#include <scripting/ScriptObjectResource.hpp>
#endif

using namespace hyperion;

extern "C"
{

    struct HypObjectInitializer
    {
        const Class* cls;
        void* nativeAddress;
    };

#pragma region HypObject

    HYP_EXPORT void HypObject_Initialize(const Class* cls, dotnet::ManagedClass* pClass, dotnet::ObjectReference* objectReference, void** outInstancePtr)
    {
        Assert(cls != nullptr);
        Assert(pClass != nullptr);
        Assert(objectReference != nullptr);
        Assert(outInstancePtr != nullptr);

        Assert(cls->UseHandles());

#ifdef HYP_DOTNET
        HypObjectPtr ptr;

        *outInstancePtr = nullptr;

        {
            // Suppress default managed object creation
            GlobalContextScope scope(HypObjectInitializerContext { cls, HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

            HypData value;

            // Set allowAbstract to true so we can use classes marked as `Abstract=true`.
            // allowing the managed class to override methods of an abstract class
            bool success = cls->CreateInstance(value, /* allowAbstract */ true);
            Assert(success, "Failed to create instance of Class '%s'", cls->GetName().LookupString());

            ptr = HypObjectPtr(cls, value.ToRef().GetPointer());

            // Ref counts are kept as 1 for Handle<T> and RC<T>, managed side is responsible for decrementing the ref count
            ptr.IncRef();

            value.Reset();
        }

        *outInstancePtr = ptr.GetPointer();

        ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>(
            ptr,
            pClass->RefCountedPtrFromThis(),
            *objectReference,
            ObjectFlags::CREATED_FROM_MANAGED);

        HypObjectBase* target = reinterpret_cast<HypObjectBase*>(ptr.GetPointer());

        target->SetScriptObjectResource(scriptObjectResource);
#endif

        /// NOTE: CREATED_FROM_MANAGED is set to true here, so we don't set keep alive to true
    }

    HYP_EXPORT uint32 HypObject_GetRefCountStrong(const Class* cls, void* nativeAddress)
    {
        Assert(cls != nullptr);
        Assert(nativeAddress != nullptr);

        HypObjectPtr hypObjectPtr = HypObjectPtr(cls, nativeAddress);

        return hypObjectPtr.GetRefCountStrong();
    }

    HYP_EXPORT void HypObject_IncRef(const Class* cls, void* nativeAddress, int8 isWeak)
    {
        Assert(cls != nullptr);
        Assert(nativeAddress != nullptr);

        HypObjectPtr hypObjectPtr = HypObjectPtr(cls, nativeAddress);
        hypObjectPtr.IncRef(isWeak);
    }

    HYP_EXPORT void HypObject_DecRef(const Class* cls, void* nativeAddress, int8 isWeak)
    {
        Assert(cls != nullptr);
        Assert(nativeAddress != nullptr);

        HypObjectPtr hypObjectPtr = HypObjectPtr(cls, nativeAddress);
        hypObjectPtr.DecRef(isWeak);
    }

#pragma endregion HypObject

} // extern "C"
