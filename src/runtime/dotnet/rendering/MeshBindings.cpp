#include <runtime/dotnet/ManagedHandle.hpp>

#include <rendering/Mesh.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion::v2;

extern "C" {
    ManagedHandle Mesh_Create()
    {
        return CreateManagedHandleFromHandle(CreateObject<Mesh>());
    }
}