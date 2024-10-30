/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Handle_Get(uint32 type_id_value, uint32 id_value, ValueStorage<HypData> *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);

    const TypeID type_id { type_id_value };

    ObjectContainerBase *container = ObjectPool::TryGetContainer(type_id);
    AssertThrow(container != nullptr);

    void *ptr = container->GetObjectPointer(id_value - 1);

    out_hyp_data->Construct(AnyRef(type_id, ptr));
}

HYP_EXPORT void Handle_Set(uint32 type_id_value, uint32 id_value, HypData *hyp_data)
{
    AssertThrow(hyp_data != nullptr);

    // const TypeID type_id { type_id_value };

    // ObjectContainerBase *container = ObjectPool::TryGetContainer(type_id);
    // AssertThrow(container != nullptr);

    HYP_NOT_IMPLEMENTED_VOID();
}

} // extern "C"