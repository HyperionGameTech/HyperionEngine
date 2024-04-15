/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ManagedHandle::IncRef(uint32 type_id)
{
    ObjectContainerBase *container = ObjectPool::TryGetContainer(TypeID { type_id });

    if (container != nullptr) {
        container->IncRefStrong(IDBase { id }.ToIndex());
    }
}

void ManagedHandle::DecRef(uint32 type_id)
{
    ObjectContainerBase *container = ObjectPool::TryGetContainer(TypeID { type_id });

    if (container != nullptr) {
        container->DecRefStrong(IDBase { id }.ToIndex());
    }

    id = IDBase().Value();
}

} // namespace hyperion::v2

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT void ManagedHandle_IncRef(uint32 type_id, ManagedHandle handle)
{
    handle.IncRef(type_id);
}

HYP_EXPORT void ManagedHandle_DecRef(uint32 type_id, ManagedHandle handle)
{
    handle.DecRef(type_id);
}
} // extern "C"