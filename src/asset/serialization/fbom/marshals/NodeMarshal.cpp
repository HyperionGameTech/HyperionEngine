/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>

#include <core/object/HypData.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Node> : public FBOMObjectMarshalerBase<Node>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Node &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("type"), FBOMData::FromUInt32(uint32(in_object.GetType())));
        
        out.SetProperty(NAME("name"), FBOMData::FromString(in_object.GetName()));

        out.SetProperty(NAME("local_transform"), FBOMData::FromObject(
            FBOMObject()
                .SetProperty(NAME("translation"), FBOMData::FromVec3f(in_object.GetLocalTransform().GetTranslation()))
                .SetProperty(NAME("rotation"), FBOMData::FromQuat4f(in_object.GetLocalTransform().GetRotation()))
                .SetProperty(NAME("scale"), FBOMData::FromVec3f(in_object.GetLocalTransform().GetScale()))
        ));

        out.SetProperty(NAME("world_transform"), FBOMData::FromObject(
            FBOMObject()
                .SetProperty(NAME("translation"), FBOMData::FromVec3f(in_object.GetWorldTransform().GetTranslation()))
                .SetProperty(NAME("rotation"), FBOMData::FromQuat4f(in_object.GetWorldTransform().GetRotation()))
                .SetProperty(NAME("scale"), FBOMData::FromVec3f(in_object.GetWorldTransform().GetScale()))
        ));

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

        if (ID<Entity> entity = in_object.GetEntity(); entity.IsValid()) {
            Handle<Entity> entity_handle { entity };
            entity_handle->SetID(entity);

            if (FBOMResult err = out.AddChild(*entity_handle)) {
                return err;
            }
        }

        for (const NodeProxy &child : in_object.GetChildren()) {
            if (!child.IsValid()) {
                continue;
            }

            if (FBOMResult err = out.AddChild(*child.Get(), FBOMObjectFlags::KEEP_UNIQUE)) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        Node::Type node_type = Node::Type::NODE;

        if (auto err = in.GetProperty("type").ReadUInt32(&node_type)) {
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
            
            FBOMObject local_transform_object;

            if (FBOMResult err = in.GetProperty("local_transform").ReadObject(local_transform_object)) {
                return err;
            }

            if (FBOMResult err = local_transform_object.GetProperty("translation").ReadVec3f(&transform.GetTranslation())) {
                return err;
            }
        
            if (FBOMResult err = local_transform_object.GetProperty("rotation").ReadQuat4f(&transform.GetRotation())) {
                return err;
            }

            if (FBOMResult err = local_transform_object.GetProperty("scale").ReadVec3f(&transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            node->SetLocalTransform(transform);
        }

        { // world transform
            Transform transform = Transform::identity;
            
            FBOMObject world_transform_object;

            if (FBOMResult err = in.GetProperty("world_transform").ReadObject(world_transform_object)) {
                return err;
            }

            if (FBOMResult err = world_transform_object.GetProperty("translation").ReadVec3f(&transform.GetTranslation())) {
                return err;
            }
        
            if (FBOMResult err = world_transform_object.GetProperty("rotation").ReadQuat4f(&transform.GetRotation())) {
                return err;
            }

            if (FBOMResult err = world_transform_object.GetProperty("scale").ReadVec3f(&transform.GetScale())) {
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
                node->AddChild(sub_object.m_deserialized_object->Get<NodeProxy>());
            } else if (sub_object.GetType().IsOrExtends("Entity")) {
                node->SetEntity(sub_object.m_deserialized_object->Get<Handle<Entity>>().GetID());
            }
        }

        out = HypData(std::move(node));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Node, FBOMMarshaler<Node>);

} // namespace hyperion::fbom