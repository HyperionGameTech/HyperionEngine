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

    HypObjectBase *hyp_object_ptr = container->GetObject(header_ptr);
    AssertThrow(hyp_object_ptr != nullptr);

    out_hyp_data->Construct(AnyRef(container->GetObjectTypeID(), hyp_object_ptr));
}

HYP_EXPORT void Handle_Set(uint32 type_id_value, uint32 id_value, HypData *hyp_data)
{
    AssertThrow(hyp_data != nullptr);

    // const TypeID type_id { type_id_value };

    // IObjectContainer *container = ObjectPool::TryGetContainer(type_id);
    // AssertThrow(container != nullptr);

    HYP_NOT_IMPLEMENTED_VOID();
}

} // extern "C"