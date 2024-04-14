#include <dotnet/runtime/ManagedHandle.hpp>

#include <rendering/Mesh.hpp>

#include <core/lib/TypeID.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT uint32 Mesh_GetTypeID()
{
    return TypeID::ForType<Mesh>().Value();
}

HYP_EXPORT void Mesh_Create(Vertex *vertices, uint32 num_vertices, uint32 *indices, uint32 num_indices, ManagedHandle *out_handle)
{
    Array<Vertex> vertices_array;
    vertices_array.Resize(num_vertices);
    Memory::MemCpy(vertices_array.Data(), vertices, num_vertices * sizeof(Vertex));

    Array<uint32> indices_array;
    indices_array.Resize(num_indices);
    Memory::MemCpy(indices_array.Data(), indices, num_indices * sizeof(uint32));

    *out_handle = CreateManagedHandleFromHandle(CreateObject<Mesh>(std::move(vertices_array), std::move(indices_array)));
}

HYP_EXPORT void Mesh_Init(ManagedHandle mesh_handle)
{
    Handle<Mesh> mesh = CreateHandleFromManagedHandle<Mesh>(mesh_handle);

    if (!mesh) {
        return;
    }

    InitObject(mesh);
}

HYP_EXPORT void Mesh_GetAABB(ManagedHandle mesh_handle, BoundingBox *out_aabb)
{
    Handle<Mesh> mesh = CreateHandleFromManagedHandle<Mesh>(mesh_handle);

    if (!mesh) {
        return;
    }

    *out_aabb = mesh->GetAABB();
}
} // extern "C"