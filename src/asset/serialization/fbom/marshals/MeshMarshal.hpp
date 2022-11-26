#ifndef HYPERION_V2_FBOM_MARSHALS_MESH_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_MESH_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Mesh.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Mesh> : public FBOMObjectMarshalerBase<Mesh>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Mesh::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Mesh &in_object, FBOMObject &out) const override
    {
        out.SetProperty("topology", FBOMUnsignedInt(), in_object.GetTopology());
        out.SetProperty("flags", FBOMUnsignedInt(), in_object.GetFlags());
        out.SetProperty("attributes", FBOMStruct(sizeof(VertexAttributeSet)), &in_object.GetVertexAttributes());
    
        // dump vertices and indices
        out.SetProperty(
            "num_vertices",
            FBOMUnsignedInt(),
            static_cast<UInt32>(in_object.GetVertices().size())
        );

        out.SetProperty(
            "vertices",
            FBOMArray(FBOMStruct(sizeof(Vertex)), in_object.GetVertices().size()),
            in_object.GetVertices().data()
        );

        out.SetProperty(
            "num_indices",
            FBOMUnsignedInt(),
            static_cast<UInt32>(in_object.GetVertices().size())
        );
    
        out.SetProperty(
            "indices",
            FBOMArray(FBOMUnsignedInt(), in_object.GetIndices().size()),
            in_object.GetIndices().data()
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Topology topology = Topology::TRIANGLES;

        if (auto err = in.GetProperty("topology").ReadUnsignedInt(&topology)) {
            return err;
        }

        Mesh::Flags flags;

        if (auto err = in.GetProperty("flags").ReadUnsignedInt(&flags)) {
            return err;
        }

        VertexAttributeSet vertex_attributes;

        if (auto err = in.GetProperty("attributes").ReadStruct(&vertex_attributes)) {
            return err;
        }

        std::vector<Vertex> vertices;

        if (const auto &vertices_property = in.GetProperty("vertices")) {
            const auto num_vertices = vertices_property.NumArrayElements(FBOMStruct(sizeof(Vertex)));

            if (num_vertices != 0) {
                vertices.resize(num_vertices);

                if (auto err = vertices_property.ReadArrayElements(FBOMStruct(sizeof(Vertex)), num_vertices, vertices.data())) {
                    return err;
                }
            }
        }

        std::vector<Mesh::Index> indices;

        if (const auto &indices_property = in.GetProperty("indices")) {
            const auto num_indices = indices_property.NumArrayElements(FBOMUnsignedInt());

            if (num_indices != 0) {
                indices.resize(num_indices);

                if (auto err = indices_property.ReadArrayElements(FBOMUnsignedInt(), num_indices, indices.data())) {
                    return err;
                }
            }
        }

        out_object = UniquePtr<Handle<Mesh>>::Construct(Engine::Get()->CreateObject<Mesh>(
            vertices,
            indices,
            topology,
            vertex_attributes,
            flags
        ));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif