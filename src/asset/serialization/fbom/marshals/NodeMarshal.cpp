/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Node> : public FBOMObjectMarshalerBase<Node>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Node &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("type"), FBOMData::FromUnsignedInt(uint32(in_object.GetType())));
        
        out.SetProperty(NAME("name"), FBOMData::FromString(in_object.GetName()));
    
        out.SetProperty(NAME("local_transform.translation"), FBOMData::FromVec3f(in_object.GetLocalTransform().GetTranslation()));
        out.SetProperty(NAME("local_transform.rotation"), FBOMData::FromQuaternion(in_object.GetLocalTransform().GetRotation()));
        out.SetProperty(NAME("local_transform.scale"), FBOMData::FromVec3f(in_object.GetLocalTransform().GetScale()));
    
        out.SetProperty(NAME("world_transform.translation"), FBOMData::FromVec3f(in_object.GetWorldTransform().GetTranslation()));
        out.SetProperty(NAME("world_transform.rotation"), FBOMData::FromQuaternion(in_object.GetWorldTransform().GetRotation()));
        out.SetProperty(NAME("world_transform.scale"), FBOMData::FromVec3f(in_object.GetWorldTransform().GetScale()));

        out.SetProperty(NAME("aabb"), FBOMStruct::Create<BoundingBox>(), &in_object.GetEntityAABB());

        switch (in_object.GetType()) {
        case Node::Type::NODE:
            // no-op
            break;
        case Node::Type::BONE:
            // TODO
            break;
        default:
            return { FBOMResult::FBOM_ERR, "Unsupported node type" }; 
        }

        if (const auto &entity = in_object.GetEntity()) {
            // @TODO Fix when entity moving between EntityManager instances is supported

            // if (auto err = out.AddChild(*entity/*, FBOM_OBJECT_FLAGS_EXTERNAL*/)) {
            //     return err;
            // }
        }

        for (const auto &child : in_object.GetChildren()) {
            if (!child) {
                continue;
            }

            if (auto err = out.AddChild(*child.Get(), FBOM_OBJECT_FLAGS_KEEP_UNIQUE)) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Node::Type node_type = Node::Type::NODE;

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&node_type)) {
            return err;
        }

        NodeProxy node;

        switch (node_type) {
        case Node::Type::NODE:
            node = NodeProxy(new Node());
            break;
        case Node::Type::BONE:
            node = NodeProxy(new Bone());
            break;
        default:
            return { FBOMResult::FBOM_ERR, "Unsupported node type" }; 
        }

        String name;
        in.GetProperty("name").ReadString(name);
        node->SetName(std::move(name));

        { // local transform
            Transform transform = Transform::identity;

            if (auto err = in.GetProperty("local_transform.translation").ReadVec3f(&transform.GetTranslation())) {
                return err;
            }
        
            if (auto err = in.GetProperty("local_transform.rotation").ReadQuaternion(&transform.GetRotation())) {
                return err;
            }

            if (auto err = in.GetProperty("local_transform.scale").ReadVec3f(&transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            node->SetLocalTransform(transform);
        }

        { // world transform
            Transform transform = Transform::identity;

            if (auto err = in.GetProperty("world_transform.translation").ReadElements(FBOMFloat(), 3, &transform.GetTranslation())) {
                return err;
            }
        
            if (auto err = in.GetProperty("world_transform.rotation").ReadElements(FBOMFloat(), 4, &transform.GetRotation())) {
                return err;
            }

            if (auto err = in.GetProperty("world_transform.scale").ReadElements(FBOMFloat(), 3, &transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            node->SetWorldTransform(transform);
        }

        { // bounding box
            BoundingBox aabb = BoundingBox::Empty();
            BoundingBox world_aabb = BoundingBox::Empty();

            if (auto err = in.GetProperty("aabb").ReadStruct<BoundingBox>(&aabb)) {
                return err;
            }

            node->SetEntityAABB(aabb);
        }

        for (auto &sub_object : *in.nodes) {
            if (sub_object.GetType().IsOrExtends("Node")) {
                node->AddChild(sub_object.deserialized.Get<Node>());
            } else if (sub_object.GetType().IsOrExtends("Entity")) {
                // @TODO: Fix when entity moving between EntityManager instances is supported

                // node_ptr->SetEntity(sub_object.deserialized.Get<Entity>());
            }
        }

        out_object = std::move(node);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Node, FBOMMarshaler<Node>);

} // namespace hyperion::fbom