/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Object.hpp>

#include <Core/utilities/GlobalContext.hpp>

#include <Core/reflection/ObjectPool.hpp>

#include <dotnet/ManagedObject.hpp>
#include <dotnet/ManagedClass.hpp>

#ifdef HYP_DOTNET
#include <scripting/ScriptObjectResource.hpp>
#endif

using namespace Hyperion;

extern "C"
{

    struct ObjectInitializer
    {
        const Class* cls;
        void* nativeAddress;
    };

#pragma region Object

    HYP_EXPORT void Object_Initialize(const Class* cls, dotnet::ManagedClass* pClass, dotnet::ObjectReference* objectReference, void** outInstancePtr)
    {
        Assert(cls != nullptr);
        Assert(pClass != nullptr);
        Assert(objectReference != nullptr);
        Assert(outInstancePtr != nullptr);

        Assert(cls->UseHandles());

#ifdef HYP_DOTNET
        ObjectBase* ptr = nullptr;

        *outInstancePtr = nullptr;

        {
            // Suppress default managed object creation
            GlobalContextScope scope(ObjectInitializerContext { cls, ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

            BoxedValue value;

            // Set allowAbstract to true so we can use classes marked as `Abstract=true`.
            // allowing the managed class to override methods of an abstract class
            bool success = cls->CreateInstance(value, /* allowAbstract */ true);
            Assert(success, "Failed to create instance of Class '{}'", cls->GetName().LookupString());

            Handle<ObjectBase>& handle = value.Get<Handle<ObjectBase>>();
            Assert(handle.IsValid() && handle->IsA(cls));

            ptr = handle.Get();

            // Ref counts are kept as 1 for Handle<T>, managed side is responsible for decrementing the ref count
            ptr->GetObjectHeader_Internal()->IncRefStrong();

            handle.Reset();
        }

        *outInstancePtr = ptr;

        ScriptObjectResource* scriptObjectResource = new ScriptObjectResource(
            ptr,
            pClass->RefCountedPtrFromThis(),
            *objectReference,
            ObjectFlags::CREATED_FROM_MANAGED);

        ptr->SetScriptObjectResource(scriptObjectResource);
#endif

        /// NOTE: CREATED_FROM_MANAGED is set to true here, so we don't set keep alive to true
    }

    HYP_EXPORT void Object_GetId(ObjectBase* obj, ObjIdBase* outId)
    {
        AssertDebug(outId != nullptr);

        *outId = obj ? obj->Id() : ObjIdBase();
    }

    HYP_EXPORT uint32 Object_GetRefCountStrong(const Class* cls, void* nativeAddress)
    {
        Assert(cls != nullptr);
        Assert(nativeAddress != nullptr);
        
        ObjectBase* obj = static_cast<ObjectBase*>(nativeAddress);
        return obj->GetObjectHeader_Internal()->GetRefCountStrong();
    }

    HYP_EXPORT void Object_IncRef(const Class* cls, void* nativeAddress, int8 isWeak)
    {
        Assert(cls != nullptr);
        Assert(nativeAddress != nullptr);
        
        ObjectBase* obj = static_cast<ObjectBase*>(nativeAddress);
        if (isWeak)
        {
            obj->GetObjectHeader_Internal()->IncRefWeak();
        }
        else
        {
            obj->AddRef();
        }
    }

    HYP_EXPORT void Object_DecRef(const Class* cls, void* nativeAddress, int8 isWeak)
    {
        Assert(cls != nullptr);
        Assert(nativeAddress != nullptr);

        ObjectBase* obj = static_cast<ObjectBase*>(nativeAddress);
        if (isWeak)
        {
            obj->GetObjectHeader_Internal()->DecRefWeak();
        }
        else
        {
            obj->Release();
        }
    }

#pragma endregion Object

} // extern "C"
