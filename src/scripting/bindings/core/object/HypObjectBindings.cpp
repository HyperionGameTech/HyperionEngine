/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/object/HypObjectPool.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    struct HypObjectInitializer
    {
        const HypClass* hypClass;
        void* nativeAddress;
    };

#pragma region HypObject

    HYP_EXPORT void HypObject_Initialize(const HypClass* hypClass, dotnet::Class* classObjectPtr, dotnet::ObjectReference* objectReference, void** outInstancePtr)
    {
        Assert(hypClass != nullptr);
        Assert(classObjectPtr != nullptr);
        Assert(objectReference != nullptr);
        Assert(outInstancePtr != nullptr);

        Assert(hypClass->UseHandles());

#ifdef HYP_DOTNET
        HypObjectPtr ptr;

        *outInstancePtr = nullptr;

        {
            // Suppress default managed object creation
            GlobalContextScope scope(HypObjectInitializerContext { hypClass, HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

            HypData value;

            // Set allowAbstract to true so we can use classes marked as `Abstract=true`.
            // allowing the managed class to override methods of an abstract class
            bool success = hypClass->CreateInstance(value, /* allowAbstract */ true);
            Assert(success, "Failed to create instance of HypClass '%s'", hypClass->GetName().LookupString());

            ptr = HypObjectPtr(hypClass, value.ToRef().GetPointer());

            // Ref counts are kept as 1 for Handle<T> and RC<T>, managed side is responsible for decrementing the ref count
            ptr.IncRef();

            value.Reset();
        }

        *outInstancePtr = ptr.GetPointer();

        ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>(
            ptr,
            classObjectPtr->RefCountedPtrFromThis(),
            *objectReference,
            ObjectFlags::CREATED_FROM_MANAGED);

        HypObjectBase* target = reinterpret_cast<HypObjectBase*>(ptr.GetPointer());

        target->SetScriptObjectResource(scriptObjectResource);
#endif

        /// NOTE: CREATED_FROM_MANAGED is set to true here, so we don't set keep alive to true
    }

    HYP_EXPORT uint32 HypObject_GetRefCountStrong(const HypClass* hypClass, void* nativeAddress)
    {
        Assert(hypClass != nullptr);
        Assert(nativeAddress != nullptr);

        HypObjectPtr hypObjectPtr = HypObjectPtr(hypClass, nativeAddress);

        return hypObjectPtr.GetRefCountStrong();
    }

    HYP_EXPORT void HypObject_IncRef(const HypClass* hypClass, void* nativeAddress, int8 isWeak)
    {
        Assert(hypClass != nullptr);
        Assert(nativeAddress != nullptr);

        HypObjectPtr hypObjectPtr = HypObjectPtr(hypClass, nativeAddress);
        hypObjectPtr.IncRef(isWeak);
    }

    HYP_EXPORT void HypObject_DecRef(const HypClass* hypClass, void* nativeAddress, int8 isWeak)
    {
        Assert(hypClass != nullptr);
        Assert(nativeAddress != nullptr);

        HypObjectPtr hypObjectPtr = HypObjectPtr(hypClass, nativeAddress);
        hypObjectPtr.DecRef(isWeak);
    }

#pragma endregion HypObject

} // extern "C"
