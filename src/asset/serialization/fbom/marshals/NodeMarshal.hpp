/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_MARSHALS_NODE_MARSHAL_HPP
#define HYPERION_FBOM_MARSHALS_NODE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
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
        out.SetProperty("type", FBOMData::FromUnsignedInt(uint32(in_object.GetType())));
        
        out.SetProperty("name", FBOMData::FromString(in_object.GetName()));
    
        out.SetProperty("local_transform.translation", FBOMData::FromVec3f(in_object.GetLocalTransform().GetTranslation()));
        out.SetProperty("local_transform.rotation", FBOMData::FromQuaternion(in_object.GetLocalTransform().GetRotation()));
        out.SetProperty("local_transform.scale", FBOMData::FromVec3f(in_object.GetLocalTransform().GetScale()));
    
        out.SetProperty("world_transform.translation", FBOMData::FromVec3f(in_object.GetWorldTransform().GetTranslation()));
        out.SetProperty("world_transform.rotation", FBOMData::FromQuaternion(in_object.GetWorldTransform().GetRotation()));
        out.SetProperty("world_transform.scale", FBOMData::FromVec3f(in_object.GetWorldTransform().GetScale()));

        out.SetProperty("local_aabb", FBOMStruct::Create<BoundingBox>(), &in_object.GetLocalAABB());
        out.SetProperty("world_aabb", FBOMStruct::Create<BoundingBox>(), &in_object.GetWorldAABB());

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

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Node::Type node_type = Node::Type::NODE;

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&node_type)) {
            return err;
        }

        UniquePtr<Node> node_ptr;

        switch (node_type) {
        case Node::Type::NODE:
            node_ptr.Reset(new Node());
            break;
        case Node::Type::BONE:
            node_ptr.Reset(new Bone());
            break;
        default:
            return { FBOMResult::FBOM_ERR, "Unsupported node type" }; 
        }

        String name;
        in.GetProperty("name").ReadString(name);
        node_ptr->SetName(std::move(name));

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
            node_ptr->SetLocalTransform(transform);
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
            node_ptr->SetWorldTransform(transform);
        }

        { // bounding box
            BoundingBox local_aabb = BoundingBox::Empty();
            BoundingBox world_aabb = BoundingBox::Empty();

            if (auto err = in.GetProperty("local_aabb").ReadStruct<BoundingBox>(&local_aabb)) {
                return err;
            }

            node_ptr->SetLocalAABB(local_aabb);

            if (auto err = in.GetProperty("world_aabb").ReadStruct<BoundingBox>(&world_aabb)) {
                return err;
            }

            node_ptr->SetWorldAABB(world_aabb);
        }

        for (auto &sub_object : *in.nodes) {
            if (sub_object.GetType().IsOrExtends("Node")) {
                node_ptr->AddChild(sub_object.deserialized.Get<Node>());
            } else if (sub_object.GetType().IsOrExtends("Entity")) {
                // @TODO: Fix when entity moving between EntityManager instances is supported

                // node_ptr->SetEntity(sub_object.deserialized.Get<Entity>());
            }
        }

        out_object = std::move(node_ptr);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::fbom

#endif