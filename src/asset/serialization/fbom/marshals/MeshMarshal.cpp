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
        out.SetProperty(NAME("topology"), uint32(in_object.GetTopology()));
        out.SetProperty(NAME("attributes"), FBOMStruct::Create<VertexAttributeSet>(), sizeof(VertexAttributeSet), &in_object.GetVertexAttributes());

        const RC<StreamedMeshData> &streamed_mesh_data = in_object.GetStreamedMeshData();

        if (streamed_mesh_data) {
            auto ref = streamed_mesh_data->AcquireRef();
            const MeshData &mesh_data = ref->GetMeshData();

            if (FBOMResult err = out.AddChild(mesh_data)) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Topology topology = Topology::TRIANGLES;

        if (FBOMResult err = in.GetProperty("topology").ReadUnsignedInt(&topology)) {
            return err;
        }

        VertexAttributeSet vertex_attributes;

        if (FBOMResult err = in.GetProperty("attributes").ReadStruct(&vertex_attributes)) {
            return err;
        }

        RC<StreamedMeshData> streamed_mesh_data;

        const auto mesh_data_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("MeshData");
        });

        if (mesh_data_it != in.nodes->End()) {
            streamed_mesh_data.Reset(new StreamedMeshData(*mesh_data_it->deserialized.Get<MeshData>()));

            return { FBOMResult::FBOM_OK };
        }

        out_object = CreateObject<Mesh>(
            std::move(streamed_mesh_data),
            topology,
            vertex_attributes
        );

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Mesh, FBOMMarshaler<Mesh>);

} // namespace hyperion::fbom