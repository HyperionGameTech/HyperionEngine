/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <scene/Mesh.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Mesh> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const Mesh& in_object = in.Get<Mesh>();

        out.SetProperty("Topology", uint32(in_object.GetTopology()));
        out.SetProperty("Attributes", FBOMStruct::Create<VertexAttributeSet>(), sizeof(VertexAttributeSet), &in_object.GetVertexAttributes());

        /// FIXME: StreamedMeshData is no longer HypObject
        StreamedMeshData* streamed_mesh_data = in_object.GetStreamedMeshData();

        if (streamed_mesh_data != nullptr)
        {
            ResourceHandle resource_handle(*streamed_mesh_data);

            const MeshData& mesh_data = streamed_mesh_data->GetMeshData();

            if (FBOMResult err = out.AddChild(mesh_data))
            {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Topology topology = TOP_TRIANGLES;

        if (FBOMResult err = in.GetProperty("Topology").ReadUInt32(&topology))
        {
            return err;
        }

        VertexAttributeSet vertex_attributes;

        if (FBOMResult err = in.GetProperty("Attributes").ReadStruct(&vertex_attributes))
        {
            return err;
        }

        RC<StreamedMeshData> streamed_mesh_data;
        ResourceHandle streamed_mesh_data_resource_handle;

        const auto mesh_data_it = in.GetChildren().FindIf([](const FBOMObject& item)
            {
                return item.GetType().IsOrExtends("MeshData");
            });

        if (mesh_data_it != in.GetChildren().End())
        {
            streamed_mesh_data.EmplaceAs<StreamedMeshData>(mesh_data_it->m_deserialized_object->Get<MeshData>(), streamed_mesh_data_resource_handle);
        }

        Handle<Mesh> mesh_handle = CreateObject<Mesh>(
            std::move(streamed_mesh_data),
            topology,
            vertex_attributes);

        streamed_mesh_data_resource_handle.Reset();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, Mesh::Class(), AnyRef(*mesh_handle)))
        {
            return err;
        }

        out = HypData(std::move(mesh_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Mesh, FBOMMarshaler<Mesh>);

} // namespace hyperion::serialization