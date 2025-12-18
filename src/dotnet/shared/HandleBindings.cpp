/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/reflection/Class.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Handle_Get(ObjectBase* ptr, ValueStorage<HypData>* outHypData)
    {
        Assert(outHypData != nullptr);
        Assert(ptr != nullptr);

        outHypData->Construct(AnyRef(ptr->GetObjectHeader_Internal()->cls->GetTypeInfo(), ptr));
    }

    HYP_EXPORT void Handle_Set(HypData* hypData, ObjectBase** outPtr)
    {
        Assert(outPtr != nullptr);

        if (hypData != nullptr)
        {
            Handle<ObjectBase>& handle = hypData->Get<Handle<ObjectBase>>();

            if (handle.IsValid())
            {
                *outPtr = handle.ptr;

                (void)handle.Release();

                return;
            }
        }

        *outPtr = nullptr;
    }

    HYP_EXPORT void Handle_Destruct(ObjectBase* ptr)
    {
        if (ptr != nullptr)
        {
            ptr->GetObjectHeader_Internal()->DecRefStrong();
        }
    }

    HYP_EXPORT uint8 WeakHandle_Lock(ObjectBase* ptr)
    {
        Assert(ptr != nullptr);

        ObjectHeader* header = ptr->GetObjectHeader_Internal();
        AssertDebug(header != nullptr);

        if (!header->TryIncRefStrong())
        {
            return 0;
        }

        return 1;
    }

    HYP_EXPORT void WeakHandle_Set(HypData* hypData, ObjectBase** outPtr)
    {
        Assert(outPtr != nullptr);

        if (hypData != nullptr)
        {
            Handle<ObjectBase>& handle = hypData->Get<Handle<ObjectBase>>();

            if (handle.IsValid())
            {
                handle.ptr->GetObjectHeader_Internal()->IncRefWeak();

                *outPtr = handle.ptr;

                handle.Reset();

                return;
            }
        }

        *outPtr = nullptr;
    }

    HYP_EXPORT void WeakHandle_Destruct(ObjectBase* ptr)
    {
        if (ptr != nullptr)
        {
            ptr->GetObjectHeader_Internal()->DecRefWeak();
        }
    }

} // extern "C"
