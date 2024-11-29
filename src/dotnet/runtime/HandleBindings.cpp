/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypData.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void Handle_Get(uint32 type_id_value, ObjectBytesBase *ptr, ValueStorage<HypData> *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);
    AssertThrow(ptr != nullptr);

    const TypeID type_id { type_id_value };

    IObjectContainer *container = ObjectPool::TryGetContainer(type_id);
    AssertThrow(container != nullptr);

    out_hyp_data->Construct(container->GetObject(ptr));
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