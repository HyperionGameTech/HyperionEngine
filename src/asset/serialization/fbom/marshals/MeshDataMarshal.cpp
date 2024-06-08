/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Mesh.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<MeshData> : public FBOMObjectMarshalerBase<MeshData>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const MeshData &in_object, FBOMObject &out) const override
    {
        auto x = ::hyperion::detail::MakeStaticString_Impl(HYP_MAKE_CONST_ARG("hello world"));

        out.SetProperty(
            Name("vertices"),
            FBOMSequence(FBOMStruct::Create<Vertex>(), in_object.vertices.Size()),
            in_object.vertices.Data()
        );
    
        out.SetProperty(
            Name("indices"),
            FBOMSequence(FBOMUnsignedInt(), in_object.indices.Size()),
            in_object.indices.Data()
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
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

        Array<uint32> indices;

        if (const auto &indices_property = in.GetProperty("indices")) {
            const auto num_indices = indices_property.NumElements(FBOMUnsignedInt());

            if (num_indices != 0) {
                indices.Resize(num_indices);

                if (auto err = indices_property.ReadElements(FBOMUnsignedInt(), num_indices, indices.Data())) {
                    return err;
                }
            }
        }

        out_object = MeshData {
            std::move(vertices),
            std::move(indices)
        };

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(MeshData, FBOMMarshaler<MeshData>);

} // namespace hyperion::fbom