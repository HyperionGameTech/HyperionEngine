/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <rendering/Mesh.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Mesh> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out)) {
            return err;
        }

        const Mesh &in_object = in.Get<Mesh>();

        out.SetProperty("Topology", uint32(in_object.GetTopology()));
        out.SetProperty("Attributes", FBOMStruct::Create<VertexAttributeSet>(), sizeof(VertexAttributeSet), &in_object.GetVertexAttributes());

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

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        Topology topology = Topology::TRIANGLES;

        if (FBOMResult err = in.GetProperty("Topology").ReadUInt32(&topology)) {
            return err;
        }

        VertexAttributeSet vertex_attributes;

        if (FBOMResult err = in.GetProperty("Attributes").ReadStruct(&vertex_attributes)) {
            return err;
        }

        RC<StreamedMeshData> streamed_mesh_data;

        const auto mesh_data_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("MeshData");
        });

        if (mesh_data_it != in.nodes->End()) {
            streamed_mesh_data.EmplaceAs<StreamedMeshData>(mesh_data_it->m_deserialized_object->Get<MeshData>());
        }

        Handle<Mesh> mesh_handle = CreateObject<Mesh>(
            std::move(streamed_mesh_data),
            topology,
            vertex_attributes
        );

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(in, Mesh::Class(), AnyRef(*mesh_handle))) {
            return err;
        }

        out = HypData(std::move(mesh_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Mesh, FBOMMarshaler<Mesh>);

} // namespace hyperion::fbom