#include <runtime/dotnet/ManagedHandle.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ManagedHandle::IncRef(UInt32 type_id)
{
    ObjectContainerBase *container = g_engine->GetObjectPool().TryGetContainer(TypeID { type_id });

    if (container != nullptr) {
        container->IncRefStrong(IDBase { id }.ToIndex());
    }
}

void ManagedHandle::DecRef(UInt32 type_id)
{
    ObjectContainerBase *container = g_engine->GetObjectPool().TryGetContainer(TypeID { type_id });

    if (container != nullptr) {
        container->DecRefStrong(IDBase { id }.ToIndex());
    }

    id = IDBase().Value();
}

} // namespace hyperion::v2

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    void ManagedHandle_IncRef(UInt32 type_id, ManagedHandle handle)
    {
        handle.IncRef(type_id);
    }

    void ManagedHandle_DecRef(UInt32 type_id, ManagedHandle handle)
    {
        handle.DecRef(type_id);
    }
}