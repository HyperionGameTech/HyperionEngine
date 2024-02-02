#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <rendering/Mesh.hpp>

#include <core/lib/TypeID.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    UInt32 Mesh_GetTypeID()
    {
        return TypeID::ForType<Mesh>().Value();
    }

    ManagedHandle Mesh_Create(Vertex *vertices, UInt32 num_vertices, UInt32 *indices, UInt32 num_indices)
    {
        Array<Vertex> vertices_array;
        vertices_array.Resize(num_vertices);
        Memory::MemCpy(vertices_array.Data(), vertices, num_vertices * sizeof(Vertex));

        Array<UInt32> indices_array;
        indices_array.Resize(num_indices);
        Memory::MemCpy(indices_array.Data(), indices, num_indices * sizeof(UInt32));

        return CreateManagedHandleFromHandle(CreateObject<Mesh>(std::move(vertices_array), std::move(indices_array)));
    }

    void Mesh_Init(ManagedHandle mesh_handle)
    {
        Handle<Mesh> mesh = CreateHandleFromManagedHandle<Mesh>(mesh_handle);

        if (!mesh) {
            return;
        }

        InitObject(mesh);
    }

    ManagedBoundingBox Mesh_GetAABB(ManagedHandle mesh_handle)
    {
        Handle<Mesh> mesh = CreateHandleFromManagedHandle<Mesh>(mesh_handle);

        if (!mesh) {
            return { };
        }

        return mesh->GetAABB();
    }
}