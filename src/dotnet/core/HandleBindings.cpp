/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Handle_Get(uint32 type_id_value, HypObjectHeader *header_ptr, ValueStorage<HypData> *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);
    AssertThrow(header_ptr != nullptr);

    IObjectContainer *container = header_ptr->container;
    AssertThrow(container != nullptr);

    out_hyp_data->Construct(AnyRef(container->GetObjectTypeID(), container->GetObjectPointer(header_ptr)));
}

HYP_EXPORT void Handle_Set(uint32 type_id_value, HypData *hyp_data, HypObjectHeader **out_header_ptr)
{
    AssertThrow(out_header_ptr != nullptr);

    if (hyp_data != nullptr) {
        AnyHandle &handle = hyp_data->Get<AnyHandle>();
        
        if (handle.IsValid()) {
            handle.header->IncRefStrong();

            *out_header_ptr = handle.header;

            return;
        }
    }

    *out_header_ptr = nullptr;
}

HYP_EXPORT void Handle_Destruct(HypObjectHeader *header_ptr)
{
    if (header_ptr != nullptr) {
        header_ptr->DecRefStrong();
    }
}

} // extern "C"