/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

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

HYP_EXPORT void ManagedHandle_IncRef(uint32 type_id, ManagedHandle handle)
{
    handle.IncRef(type_id);
}

HYP_EXPORT void ManagedHandle_DecRef(uint32 type_id, ManagedHandle handle)
{
    handle.DecRef(type_id);
}

HYP_EXPORT uint32 ManagedHandle_GetRefCountStrong(uint32 type_id, ManagedHandle handle)
{
    return handle.GetRefCountStrong(type_id);
}

HYP_EXPORT uint32 ManagedHandle_GetRefCountWeak(uint32 type_id, ManagedHandle handle)
{
    return handle.GetRefCountWeak(type_id);
}

} // extern "C"