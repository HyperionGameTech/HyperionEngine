/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/reflection/Class.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void Handle_GetId(ObjectBase* pObject, ObjIdBase* pOutId)
    {
        AssertDebug(pOutId != nullptr);

        *pOutId = pObject ? pObject->Id() : ObjIdBase();
    }

    HYP_EXPORT void Handle_Get(ObjectBase* pObject, ValueStorage<BoxedValue>* pOutBoxed)
    {
        Assert(pOutBoxed != nullptr);
        Assert(pObject != nullptr);

        pOutBoxed->Construct(AnyRef(pObject->GetObjectHeader_Internal()->cls->GetTypeInfo(), pObject));
    }

    HYP_EXPORT void Handle_Set(BoxedValue* pBoxed, ObjectBase** pOutObject)
    {
        Assert(pOutObject != nullptr);

        if (pBoxed != nullptr)
        {
            Handle<ObjectBase>& handle = pBoxed->Get<Handle<ObjectBase>>();

            if (handle.IsValid())
            {
                *pOutObject = handle.ptr;

                (void)handle.Release();

                return;
            }
        }

        *pOutObject = nullptr;
    }

    HYP_EXPORT void Handle_Destruct(ObjectBase* pOutObject)
    {
        if (pOutObject != nullptr)
        {
            pOutObject->GetObjectHeader_Internal()->DecRefStrong();
        }
    }

    HYP_EXPORT uint8 WeakHandle_Lock(ObjectBase* pObject)
    {
        Assert(pObject != nullptr);

        ObjectHeader* header = pObject->GetObjectHeader_Internal();
        AssertDebug(header != nullptr);

        if (!header->TryIncRefStrong())
        {
            return 0;
        }

        return 1;
    }

    HYP_EXPORT void WeakHandle_Set(BoxedValue* pBoxed, ObjectBase** pOutObject)
    {
        Assert(pOutObject != nullptr);

        if (pBoxed != nullptr)
        {
            Handle<ObjectBase>& handle = pBoxed->Get<Handle<ObjectBase>>();

            if (handle.IsValid())
            {
                handle.ptr->GetObjectHeader_Internal()->IncRefWeak();

                *pOutObject = handle.ptr;

                handle.Reset();

                return;
            }
        }

        *pOutObject = nullptr;
    }

    HYP_EXPORT void WeakHandle_Destruct(ObjectBase* pObject)
    {
        if (pObject != nullptr)
        {
            pObject->GetObjectHeader_Internal()->DecRefWeak();
        }
    }

} // extern "C"
