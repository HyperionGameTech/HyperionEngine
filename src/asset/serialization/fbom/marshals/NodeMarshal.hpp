#ifndef HYPERION_V2_FBOM_MARSHALS_NODE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_NODE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/EntityMarshal.hpp>
#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Node> : public FBOMObjectMarshalerBase<Node>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("Node");
    }

    virtual FBOMResult Serialize(const Node &in_object, FBOMObject &out) const override
    {
        out.SetProperty("type", FBOMUnsignedInt(), in_object.GetType());

        out.SetProperty("name", FBOMString(), in_object.GetName().Size(), in_object.GetName().Data());
    
        out.SetProperty("local_transform.translation", FBOMVec3f(), &in_object.GetLocalTransform().GetTranslation());
        out.SetProperty("local_transform.rotation", FBOMQuaternion(), &in_object.GetLocalTransform().GetRotation());
        out.SetProperty("local_transform.scale", FBOMVec3f(), &in_object.GetLocalTransform().GetScale());
    
        out.SetProperty("world_transform.translation", FBOMVec3f(), &in_object.GetWorldTransform().GetTranslation());
        out.SetProperty("world_transform.rotation", FBOMQuaternion(), &in_object.GetWorldTransform().GetRotation());
        out.SetProperty("world_transform.scale", FBOMVec3f(), &in_object.GetWorldTransform().GetScale());

        out.SetProperty("local_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetLocalAABB());
        out.SetProperty("world_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetWorldAABB());

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
            if (auto err = out.AddChild(*entity, false)) {
                return err;
            }
        }

        for (const auto &child : in_object.GetChildren()) {
            if (!child) {
                continue;
            }

            if (auto err = out.AddChild(*child.Get())) {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(Engine *engine, const FBOMObject &in, UniquePtr<Node> &out_object) const override
    {
        Node::Type node_type = Node::Type::NODE;

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&node_type)) {
            return err;
        }

        switch (node_type) {
        case Node::Type::NODE:
            out_object.Reset(new Node());
            break;
        case Node::Type::BONE:
            out_object.Reset(new Bone());
            break;
        default:
            return { FBOMResult::FBOM_ERR, "Unsupported node type" }; 
        }

        String name;
        in.GetProperty("name").ReadString(name);
        out_object->SetName(std::move(name));

        { // local transform
            Transform transform = Transform::identity;

            if (auto err = in.GetProperty("local_transform.translation").ReadArrayElements(FBOMFloat(), 3, &transform.GetTranslation())) {
                return err;
            }
        
            if (auto err = in.GetProperty("local_transform.rotation").ReadArrayElements(FBOMFloat(), 4, &transform.GetRotation())) {
                return err;
            }

            if (auto err = in.GetProperty("local_transform.scale").ReadArrayElements(FBOMFloat(), 3, &transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            out_object->SetLocalTransform(transform);
        }

        { // world transform
            Transform transform = Transform::identity;

            if (auto err = in.GetProperty("world_transform.translation").ReadArrayElements(FBOMFloat(), 3, &transform.GetTranslation())) {
                return err;
            }
        
            if (auto err = in.GetProperty("world_transform.rotation").ReadArrayElements(FBOMFloat(), 4, &transform.GetRotation())) {
                return err;
            }

            if (auto err = in.GetProperty("world_transform.scale").ReadArrayElements(FBOMFloat(), 3, &transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            out_object->SetWorldTransform(transform);
        }

        { // bounding box
            BoundingBox local_aabb = BoundingBox::empty;
            BoundingBox world_aabb = BoundingBox::empty;

            if (auto err = in.GetProperty("local_aabb").ReadStruct(sizeof(BoundingBox), &local_aabb)) {
                return err;
            }

            out_object->SetLocalAABB(local_aabb);

            if (auto err = in.GetProperty("world_aabb").ReadStruct(sizeof(BoundingBox), &world_aabb)) {
                return err;
            }

            out_object->SetWorldAABB(world_aabb);
        }

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Node")) {
                out_object->AddChild(node.deserialized.Get<Node>());
            } else if (node.GetType().IsOrExtends("Entity")) {
                out_object->SetEntity(node.deserialized.Get<Entity>());
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif