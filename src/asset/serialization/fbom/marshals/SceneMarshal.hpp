/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FBOM_MARSHALS_SCENE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SCENE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <asset/serialization/fbom/marshals/NodeMarshal.hpp>
#include <asset/serialization/fbom/marshals/CameraMarshal.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

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

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto scene_handle = UniquePtr<Handle<Scene>>::Construct(CreateObject<Scene>(Handle<Camera>()));

        Name name;
        in.GetProperty("name").ReadName(&name);

        (*scene_handle)->SetName(name);

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Node")) {
                (*scene_handle)->GetRoot().AddChild(node.deserialized.Get<Node>());
                // out_object->SetRoot(node.deserialized.Get<Node>());
            } else if (node.GetType().IsOrExtends("Camera")) {
                (*scene_handle)->SetCamera(node.deserialized.Get<Camera>());
            }
        }

        out_object = std::move(scene_handle);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif