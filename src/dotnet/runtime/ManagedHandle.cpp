/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

#include <core/object/HypData.hpp>

namespace hyperion {

void ManagedHandle::IncRef(uint32 type_id)
{
    AssertThrow(id != 0);

    ObjectContainerBase *container = ObjectPool::TryGetContainer(TypeID { type_id });
    AssertThrow(container != nullptr);

    container->IncRefStrong(IDBase { id }.ToIndex());
}

void ManagedHandle::DecRef(uint32 type_id)
{
    AssertThrow(id != 0);

    ObjectContainerBase *container = ObjectPool::TryGetContainer(TypeID { type_id });
    AssertThrow(container != nullptr);

    container->DecRefStrong(IDBase { id }.ToIndex());

    id = IDBase().Value();
}

uint32 ManagedHandle::GetRefCountStrong(uint32 type_id) const
{
    if (!id) {
        return 0;
    }

    ObjectContainerBase *container = ObjectPool::TryGetContainer(TypeID { type_id });
    AssertThrow(container != nullptr);

    return container->GetRefCountStrong(IDBase { id }.ToIndex());
}

uint32 ManagedHandle::GetRefCountWeak(uint32 type_id) const
{
    if (!id) {
        return 0;
    }

    ObjectContainerBase *container = ObjectPool::TryGetContainer(TypeID { type_id });
    AssertThrow(container != nullptr);

    return container->GetRefCountWeak(IDBase { id }.ToIndex());
}

} // namespace hyperion

using namespace hyperion;

extern "C" {

HYP_EXPORT void ManagedHandle_Get(uint32 type_id_value, uint32 id_value, HypData *out_hyp_data)
{
    AssertThrow(out_hyp_data != nullptr);

    const TypeID type_id { type_id_value };

    ObjectContainerBase *container = ObjectPool::TryGetContainer(type_id);
    AssertThrow(container != nullptr);

    void *ptr = container->GetObjectPointer(id_value - 1);

    *out_hyp_data = HypData(AnyRef(type_id, ptr));
}

HYP_EXPORT void ManagedHandle_Set(uint32 type_id_value, uint32 id_value, HypData *hyp_data)
{
    AssertThrow(hyp_data != nullptr);

    // const TypeID type_id { type_id_value };

    // ObjectContainerBase *container = ObjectPool::TryGetContainer(type_id);
    // AssertThrow(container != nullptr);

    HYP_NOT_IMPLEMENTED_VOID();
}

} // extern "C"