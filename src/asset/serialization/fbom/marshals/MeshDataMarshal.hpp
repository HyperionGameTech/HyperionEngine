#ifndef HYPERION_V2_FBOM_MARSHALS_MESH_DATA_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_MESH_DATA_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Mesh.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<MeshData> : public FBOMObjectMarshalerBase<MeshData>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const MeshData &in_object, FBOMObject &out) const override
    {
        out.SetProperty(
            "vertices",
            FBOMArray(FBOMStruct(sizeof(Vertex)), in_object.vertices.Size()),
            in_object.vertices.Data()
        );
    
        out.SetProperty(
            "indices",
            FBOMArray(FBOMUnsignedInt(), in_object.indices.Size()),
            in_object.indices.Data()
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Array<Vertex> vertices;

        if (const auto &vertices_property = in.GetProperty("vertices")) {
            const auto num_vertices = vertices_property.NumArrayElements(FBOMStruct(sizeof(Vertex)));

            if (num_vertices != 0) {
                vertices.Resize(num_vertices);

                if (auto err = vertices_property.ReadArrayElements(FBOMStruct(sizeof(Vertex)), num_vertices, vertices.Data())) {
                    return err;
                }
            }
        }

        Array<uint32> indices;

        if (const auto &indices_property = in.GetProperty("indices")) {
            const auto num_indices = indices_property.NumArrayElements(FBOMUnsignedInt());

            if (num_indices != 0) {
                indices.Resize(num_indices);

                if (auto err = indices_property.ReadArrayElements(FBOMUnsignedInt(), num_indices, indices.Data())) {
                    return err;
                }
            }
        }

        out_object = UniquePtr<MeshData>::Construct(MeshData {
            std::move(vertices),
            std::move(indices)
        });

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif