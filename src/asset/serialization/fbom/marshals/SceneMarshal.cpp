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
        out.SetProperty("name", FBOMName(), in_object.GetName());

        // for (auto &node : in_object.GetRoot().GetChildren()) {
        //     out.AddChild(*node.Get());
        // }

        if (in_object.GetRoot()) {
            out.AddChild(*in_object.GetRoot().Get(), FBOM_OBJECT_FLAGS_KEEP_UNIQUE);
        }

        if (auto *camera = in_object.GetCamera().Get()) {
            out.AddChild(*camera);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Handle<Scene> scene_handle = CreateObject<Scene>(Handle<Camera>());

        Name name;
        in.GetProperty("name").ReadName(&name);

        scene_handle->SetName(name);

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Node")) {
                scene_handle->SetRoot(node.deserialized.Get<Node>());
            } else if (node.GetType().IsOrExtends("Camera")) {
                scene_handle->SetCamera(node.deserialized.Get<Camera>());
            }
        }

        out_object = std::move(scene_handle);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Scene, FBOMMarshaler<Scene>);

} // namespace hyperion::fbom