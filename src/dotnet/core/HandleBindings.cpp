/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Handle_Get(HypObjectBase *ptr, ValueStorage<HypData> *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);
    AssertThrow(ptr != nullptr);

    HypObjectHeader *header = ptr->GetObjectHeader_Internal();

    IObjectContainer *container = header->container;
    AssertThrow(container != nullptr);

    out_hyp_data->Construct(AnyRef(container->GetObjectTypeID(), container->GetObjectPointer(header)));
}

HYP_EXPORT void Handle_Set(HypData *hyp_data, HypObjectBase **out_ptr)
{
    AssertThrow(out_ptr != nullptr);

    if (hyp_data != nullptr) {
        AnyHandle &handle = hyp_data->Get<AnyHandle>();

        if (handle.IsValid()) {
            handle.ptr->GetObjectHeader_Internal()->IncRefStrong();

            *out_ptr = handle.ptr;

            return;
        }
    }

    *out_ptr = nullptr;
}

HYP_EXPORT void Handle_Destruct(HypObjectBase *ptr)
{
    if (ptr != nullptr) {
        ptr->GetObjectHeader_Internal()->DecRefStrong();
    }
}

HYP_EXPORT uint8 WeakHandle_Lock(HypObjectBase *ptr)
{
    AssertThrow(ptr != nullptr);

    if (ptr->GetObjectHeader_Internal()->GetRefCountStrong() == 0) {
        return 0;
    }

    ptr->GetObjectHeader_Internal()->IncRefStrong();

    return 1;
}

HYP_EXPORT void WeakHandle_Set(HypData *hyp_data, HypObjectBase **out_ptr)
{
    AssertThrow(out_ptr != nullptr);

    if (hyp_data != nullptr) {
        AnyHandle &handle = hyp_data->Get<AnyHandle>();

        if (handle.IsValid()) {
            handle.ptr->GetObjectHeader_Internal()->IncRefWeak();

            *out_ptr = handle.ptr;

            return;
        }
    }

    *out_ptr = nullptr;
}

HYP_EXPORT void WeakHandle_Destruct(HypObjectBase *ptr)
{
    if (ptr != nullptr) {
        ptr->GetObjectHeader_Internal()->DecRefWeak();
    }
}

} // extern "C"