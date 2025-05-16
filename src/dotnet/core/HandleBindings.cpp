/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Handle_Get(uint32 type_id_value, HypObjectBase *ptr, ValueStorage<HypData> *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);
    AssertThrow(ptr != nullptr);

    HypObjectHeader *header = ptr->GetObjectHeader_Internal();

    IObjectContainer *container = header->container;
    AssertThrow(container != nullptr);

    out_hyp_data->Construct(AnyRef(container->GetObjectTypeID(), container->GetObjectPointer(header)));
}

HYP_EXPORT void Handle_Set(uint32 type_id_value, HypData *hyp_data, HypObjectBase **out_ptr)
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

} // extern "C"