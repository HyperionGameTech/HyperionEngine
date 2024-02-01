#include <runtime/dotnet/ManagedHandle.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void ManagedHandle::Dispose()
{
    ObjectContainerBase *container = g_engine->GetObjectPool().TryGetContainer(TypeID { type_id });

    if (container != nullptr) {
        container->DecRefStrong(IDBase { id }.ToIndex());
    }

    id = IDBase().Value();
    type_id = TypeID::ForType<void>().Value();
}

} // namespace hyperion::v2

extern "C" {
    void ManagedHandle_Dispose(hyperion::v2::ManagedHandle handle)
    {
        handle.Dispose();
    }

    hyperion::UInt32 ManagedHandle_GetID(hyperion::v2::ManagedHandle handle)
    {
        return handle.id;
    }
}