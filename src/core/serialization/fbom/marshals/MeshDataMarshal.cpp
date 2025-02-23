/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>

#include <rendering/Mesh.hpp>

#include <core/object/HypData.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<MeshData> : public FBOMObjectMarshalerBase<MeshData>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const MeshData &in_object, FBOMObject &out) const override
    {
        out.SetProperty(
            "Vertices",
            FBOMSequence(FBOMStruct::Create<Vertex>(), in_object.vertices.Size()),
            in_object.vertices.ByteSize(),
            in_object.vertices.Data()
        );
    
        out.SetProperty(
            "Indices",
            FBOMSequence(FBOMUInt32(), in_object.indices.Size()),
            in_object.indices.ByteSize(),
            in_object.indices.Data()
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        Array<Vertex> vertices;

        if (const FBOMData &vertices_property = in.GetProperty("Vertices")) {
            const SizeType num_vertices = vertices_property.NumElements(FBOMStruct::Create<Vertex>());

            if (num_vertices != 0) {
                vertices.Resize(num_vertices);

                if (FBOMResult err = vertices_property.ReadElements(FBOMStruct::Create<Vertex>(), num_vertices, vertices.Data())) {
                    return err;
                }
            }
        } else {
            return { FBOMResult::FBOM_ERR, String("vertices property invalid on object ") + in.ToString(false) };
        }

        Array<uint32> indices;

        if (const FBOMData &indices_property = in.GetProperty("Indices")) {
            const SizeType num_indices = indices_property.NumElements(FBOMUInt32());

            if (num_indices != 0) {
                indices.Resize(num_indices);

                if (FBOMResult err = indices_property.ReadElements(FBOMUInt32(), num_indices, indices.Data())) {
                    return err;
                }
            }
        } else {
            return { FBOMResult::FBOM_ERR, "indices property invalid" };
        }

        out = HypData(MeshData {
            std::move(vertices),
            std::move(indices)
        });

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(MeshData, FBOMMarshaler<MeshData>);

} // namespace hyperion::fbom