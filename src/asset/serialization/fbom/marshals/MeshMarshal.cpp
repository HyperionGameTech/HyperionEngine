/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Mesh.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Mesh> : public FBOMObjectMarshalerBase<Mesh>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Mesh &in_object, FBOMObject &out) const override
    {
        out.SetProperty("topology", FBOMUnsignedInt(), in_object.GetTopology());
        out.SetProperty("attributes", FBOMStruct::Create<VertexAttributeSet>(), &in_object.GetVertexAttributes());

        const RC<StreamedMeshData> &streamed_mesh_data = in_object.GetStreamedMeshData();

        if (streamed_mesh_data) {
            auto ref = streamed_mesh_data->AcquireRef();
            const MeshData &mesh_data = ref->GetMeshData();

            // dump vertices and indices
            out.SetProperty(
                "num_vertices",
                FBOMUnsignedInt(),
                uint32(mesh_data.vertices.Size())
            );

            out.SetProperty(
                "vertices",
                FBOMSequence(FBOMStruct::Create<Vertex>(), mesh_data.vertices.Size()),
                mesh_data.vertices.Data()
            );

            out.SetProperty(
                "num_indices",
                FBOMUnsignedInt(),
                uint32(mesh_data.indices.Size())
            );
        
            out.SetProperty(
                "indices",
                FBOMSequence(FBOMUnsignedInt(), mesh_data.indices.Size()),
                mesh_data.indices.Data()
            );
        } else {
            out.SetProperty(
                "num_vertices",
                FBOMUnsignedInt(),
                0
            );

            out.SetProperty(
                "vertices",
                FBOMSequence(FBOMStruct::Create<Vertex>(), 0),
                nullptr
            );

            out.SetProperty(
                "num_indices",
                FBOMUnsignedInt(),
                static_cast<uint32>(0)
            );
        
            out.SetProperty(
                "indices",
                FBOMSequence(FBOMUnsignedInt(), 0),
                nullptr
            );
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Topology topology = Topology::TRIANGLES;

        if (auto err = in.GetProperty("topology").ReadUnsignedInt(&topology)) {
            return err;
        }

        VertexAttributeSet vertex_attributes;

        if (auto err = in.GetProperty("attributes").ReadStruct(&vertex_attributes)) {
            return err;
        }

        Array<Vertex> vertices;

        if (const auto &vertices_property = in.GetProperty("vertices")) {
            const auto num_vertices = vertices_property.NumElements(FBOMStruct::Create<Vertex>());

            if (num_vertices != 0) {
                vertices.Resize(num_vertices);

                if (auto err = vertices_property.ReadElements(FBOMStruct::Create<Vertex>(), num_vertices, vertices.Data())) {
                    return err;
                }
            }
        }

        Array<Mesh::Index> indices;

        if (const auto &indices_property = in.GetProperty("indices")) {
            const auto num_indices = indices_property.NumElements(FBOMUnsignedInt());

            if (num_indices != 0) {
                indices.Resize(num_indices);

                if (auto err = indices_property.ReadElements(FBOMUnsignedInt(), num_indices, indices.Data())) {
                    return err;
                }
            }
        }

        out_object = CreateObject<Mesh>(
            vertices,
            indices,
            topology,
            vertex_attributes
        );

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Mesh, FBOMMarshaler<Mesh>);

} // namespace hyperion::fbom