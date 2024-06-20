/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Scene> : public FBOMObjectMarshalerBase<Scene>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Scene &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("name"), FBOMData::FromName(in_object.GetName()));

        if (in_object.GetRoot()) {
            if (FBOMResult err = out.AddChild(*in_object.GetRoot().Get(), FBOMObjectFlags::KEEP_UNIQUE)) {
                return err;
            }
        }

        if (const Handle<Camera> &camera = in_object.GetCamera()) {
            if (FBOMResult err = out.AddChild(*camera)) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Handle<Scene> scene_handle = CreateObject<Scene>(Handle<Camera>());

        Name name;

        if (FBOMResult err = in.GetProperty("name").ReadName(&name)) {
            return err;
        }

        scene_handle->SetName(name);

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Node")) {
                scene_handle->SetRoot(node.m_deserialized_object->Get<Node>());
            } else if (node.GetType().IsOrExtends("Camera")) {
                scene_handle->SetCamera(node.m_deserialized_object->Get<Camera>());
            }
        }

        out_object = std::move(scene_handle);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Scene, FBOMMarshaler<Scene>);

} // namespace hyperion::fbom